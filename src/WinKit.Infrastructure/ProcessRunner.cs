using System.Diagnostics;
using System.Text;

namespace WinKit.Infrastructure;

public sealed record ProcessRunResult(int ExitCode, string StandardOutput, string StandardError)
{
    public bool Succeeded => ExitCode == 0;
}

/// <summary>
/// Runs a controlled, allow-listed external process (e.g. ipconfig, netsh,
/// tracert) asynchronously and captures its output. This is the single
/// choke point services use instead of shelling out ad-hoc, so all process
/// execution is async, cancellable, and never blocks the UI thread.
/// </summary>
public static class ProcessRunner
{
    public static async Task<ProcessRunResult> RunAsync(
        string fileName,
        string arguments,
        CancellationToken cancellationToken = default,
        bool elevated = false)
    {
        var startInfo = new ProcessStartInfo(fileName, arguments)
        {
            RedirectStandardOutput = !elevated,
            RedirectStandardError = !elevated,
            UseShellExecute = elevated,
            CreateNoWindow = true,
            Verb = elevated ? "runas" : string.Empty
        };

        using var process = new Process { StartInfo = startInfo, EnableRaisingEvents = true };

        var stdOut = new StringBuilder();
        var stdErr = new StringBuilder();

        if (!elevated)
        {
            process.OutputDataReceived += (_, e) => { if (e.Data is not null) stdOut.AppendLine(e.Data); };
            process.ErrorDataReceived += (_, e) => { if (e.Data is not null) stdErr.AppendLine(e.Data); };
        }

        process.Start();

        if (!elevated)
        {
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
        }

        await using var registration = cancellationToken.Register(() =>
        {
            try
            {
                if (!process.HasExited)
                {
                    process.Kill(entireProcessTree: true);
                }
            }
            catch (InvalidOperationException)
            {
                // Process already exited between the check and the kill.
            }
        });

        await process.WaitForExitAsync(cancellationToken);

        return new ProcessRunResult(process.ExitCode, stdOut.ToString(), stdErr.ToString());
    }
}
