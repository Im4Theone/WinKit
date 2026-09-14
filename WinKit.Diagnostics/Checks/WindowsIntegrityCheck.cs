using WinKit.Core.Models;
using WinKit.Infrastructure;

namespace WinKit.Diagnostics.Checks;

/// <summary>
/// Asks DISM whether the Windows component store is already flagged as corrupted.
/// This is the fast "/CheckHealth" query (near-instant, no admin rights needed) rather
/// than "/ScanHealth" (a multi-minute deep scan) - appropriate for a check that runs
/// as part of every scan. The Repair action runs a full RestoreHealth + SFC pass.
/// </summary>
public sealed class WindowsIntegrityCheck : IDiagnosticCheck
{
    public string Id => DiagnosticCheckIds.WindowsIntegrity;
    public string Name => "Windows Integrity";
    public DiagnosticCategory Category => DiagnosticCategory.WindowsIntegrity;

    public async Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        if (!EnglishOutputGuard.IsExpected)
        {
            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.NotApplicable,
                Summary = "This check needs an English-language Windows installation to reliably read DISM's output.",
                TechnicalDetail = $"Installed UI culture: {EnglishOutputGuard.CurrentUiCultureName}."
            };
        }

        ProcessRunResult result;
        try
        {
            result = await ProcessRunner.RunAsync(
                "dism.exe", "/Online /Cleanup-Image /CheckHealth", cancellationToken);
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            return Inconclusive(ex.Message);
        }

        var output = result.StandardOutput;

        if (result.Succeeded && output.Contains("No component store corruption detected", StringComparison.OrdinalIgnoreCase))
        {
            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.Passed,
                Summary = "No known corruption in the Windows component store."
            };
        }

        if (!result.Succeeded && string.IsNullOrWhiteSpace(output) && string.IsNullOrWhiteSpace(result.StandardError))
        {
            return Inconclusive($"dism.exe exited with code {result.ExitCode} and produced no output.");
        }

        return new DiagnosticCheckResult
        {
            CheckId = Id,
            Name = Name,
            Category = Category,
            Status = DiagnosticStatus.Failed,
            Severity = DiagnosticSeverity.High,
            Summary = "The Windows component store is flagged as corrupted.",
            TechnicalDetail = string.IsNullOrWhiteSpace(output) ? result.StandardError : output,
            RecommendedAction = "Run DISM RestoreHealth and System File Checker to repair Windows system files.",
            FixAction = new DiagnosticFixAction
            {
                Label = "Repair",
                Kind = DiagnosticActionKind.Fix,
                RequiresElevation = true,
                ConfirmationMessage = "This runs DISM (RestoreHealth) and the System File Checker to repair Windows " +
                                      "system files. It can take 10-20 minutes, needs an internet connection, and " +
                                      "requires administrator permission. Don't turn off your PC while it runs.",
            }
        };
    }

    private DiagnosticCheckResult Inconclusive(string detail) => new()
    {
        CheckId = Id,
        Name = Name,
        Category = Category,
        Status = DiagnosticStatus.Informational,
        Summary = "Could not determine Windows component store health.",
        TechnicalDetail = detail
    };
}
