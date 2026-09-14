using System.Text;
using WinKit.Core.Models;
using WinKit.Infrastructure;

namespace WinKit.Diagnostics.Fixers;

/// <summary>
/// Runs DISM RestoreHealth followed by SFC scannow, both elevated. ProcessRunner
/// can't capture stdout for an elevated child process (UseShellExecute is required
/// for the "runas" verb, which is incompatible with output redirection), so this
/// routes each command through "cmd /c command > tempfile 2>&1" and reads the file
/// back afterwards - preserving both the UAC prompt and the ability to show real output.
/// </summary>
public sealed class WindowsIntegrityFixer : IDiagnosticFixer
{
    private static readonly TimeSpan CommandTimeout = TimeSpan.FromMinutes(30);

    public string CheckId => DiagnosticCheckIds.WindowsIntegrity;

    public async Task<OperationResult<string>> FixAsync(IProgress<string>? progress, CancellationToken cancellationToken)
    {
        if (!EnglishOutputGuard.IsExpected)
        {
            return OperationResult<string>.Fail(
                "This repair needs an English-language Windows installation to reliably read DISM/SFC output.",
                $"Installed UI culture: {EnglishOutputGuard.CurrentUiCultureName}.");
        }

        try
        {
            progress?.Report("Running DISM /RestoreHealth (this can take several minutes)...");
            var (dismExitCode, dismOutput) = await RunElevatedCapturedAsync(
                "dism.exe", "/Online /Cleanup-Image /RestoreHealth", cancellationToken);

            if (dismExitCode != 0)
            {
                return OperationResult<string>.Fail(
                    "DISM RestoreHealth failed. System File Checker was not run.",
                    Truncate(dismOutput));
            }

            progress?.Report("Running System File Checker (sfc /scannow)...");
            var (sfcExitCode, sfcOutput) = await RunElevatedCapturedAsync("sfc.exe", "/scannow", cancellationToken);

            var combinedOutput = $"--- DISM RestoreHealth ---{Environment.NewLine}{dismOutput}" +
                                  $"{Environment.NewLine}{Environment.NewLine}--- SFC /scannow ---{Environment.NewLine}{sfcOutput}";

            var repaired = sfcOutput.Contains("successfully repaired", StringComparison.OrdinalIgnoreCase);
            var clean = sfcOutput.Contains("did not find any integrity violations", StringComparison.OrdinalIgnoreCase);
            var unrepairable = sfcOutput.Contains("unable to fix", StringComparison.OrdinalIgnoreCase);

            if (sfcExitCode == 0 && (repaired || clean))
            {
                var summary = repaired
                    ? "DISM repaired the component store and SFC successfully repaired corrupted files."
                    : "DISM repaired the component store. SFC found no further integrity violations.";
                return OperationResult<string>.Ok(Truncate(combinedOutput), summary);
            }

            if (unrepairable)
            {
                return OperationResult<string>.Fail(
                    "SFC found corrupted files it could not repair automatically. " +
                    "Check %windir%\\Logs\\CBS\\CBS.log for details.",
                    Truncate(combinedOutput));
            }

            return OperationResult<string>.Fail(
                $"SFC exited with an unexpected result (code {sfcExitCode}).", Truncate(combinedOutput));
        }
        catch (TimeoutException ex)
        {
            // Distinguished from a user-initiated cancellation below: this is *our* internal
            // per-command budget expiring, not the caller's token being cancelled.
            return OperationResult<string>.Fail("The repair timed out and was stopped.", ex.Message);
        }
    }

    private static async Task<(int ExitCode, string Output)> RunElevatedCapturedAsync(
        string exeName, string exeArguments, CancellationToken cancellationToken)
    {
        // Each command gets its own full timeout budget - DISM RestoreHealth and SFC /scannow
        // can each legitimately take 20-30 minutes; sharing one clock across both risked
        // cutting off a still-progressing SFC pass just because DISM used most of the budget.
        using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeoutCts.CancelAfter(CommandTimeout);

        var tempFile = Path.Combine(Path.GetTempPath(), $"winkit-{Path.GetFileNameWithoutExtension(exeName)}-{Guid.NewGuid():N}.log");

        try
        {
            var cmdArguments = $"/c \"{exeName} {exeArguments} > \"{tempFile}\" 2>&1\"";
            var result = await ProcessRunner.RunAsync("cmd.exe", cmdArguments, timeoutCts.Token, elevated: true);
            var output = ReadCapturedOutput(tempFile);
            return (result.ExitCode, output);
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            // timeoutCts fired on its own (not because the caller's token was cancelled).
            throw new TimeoutException($"{exeName} did not finish within {CommandTimeout.TotalMinutes:0} minutes.");
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            // The user declined the UAC prompt.
            return (-1, ex.Message);
        }
        finally
        {
            TryDelete(tempFile);
        }
    }

    private static string ReadCapturedOutput(string path)
    {
        if (!File.Exists(path))
        {
            return string.Empty;
        }

        byte[] bytes;
        try
        {
            bytes = File.ReadAllBytes(path);
        }
        catch (IOException)
        {
            return string.Empty;
        }

        if (bytes.Length == 0)
        {
            return string.Empty;
        }

        // sfc.exe (and some DISM builds) write UTF-16LE when their output is redirected
        // to a file rather than a console; detect that by the density of zero bytes
        // at odd offsets, which is characteristic of ASCII-range UTF-16LE text.
        var sampleSize = Math.Min(bytes.Length, 200);
        var zeroCount = 0;
        for (var i = 1; i < sampleSize; i += 2)
        {
            if (bytes[i] == 0)
            {
                zeroCount++;
            }
        }

        var looksUtf16 = zeroCount > sampleSize / 4;
        var text = looksUtf16 ? Encoding.Unicode.GetString(bytes) : Encoding.UTF8.GetString(bytes);
        return text.Replace("\0", string.Empty).Trim();
    }

    private static void TryDelete(string path)
    {
        try
        {
            if (File.Exists(path))
            {
                File.Delete(path);
            }
        }
        catch (IOException)
        {
            // Best-effort cleanup of a uniquely-named temp file; harmless if it lingers.
        }
    }

    private static string Truncate(string text, int maxLength = 4000) =>
        text.Length <= maxLength ? text : text[..maxLength] + $"{Environment.NewLine}... (truncated)";
}
