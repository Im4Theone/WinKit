using WinKit.Cleanup.Models;
using WinKit.Cleanup.Services;
using WinKit.Core.Models;

namespace WinKit.Diagnostics.Checks;

/// <summary>Reuses the existing Cleanup scanner instead of re-walking these directories itself.</summary>
public sealed class TempFilesCheck : IDiagnosticCheck
{
    private const long WarningThresholdBytes = 2L * 1024 * 1024 * 1024; // 2 GB

    private readonly ICleanupScanner _cleanupScanner;

    public TempFilesCheck(ICleanupScanner cleanupScanner)
    {
        _cleanupScanner = cleanupScanner;
    }

    public string Id => DiagnosticCheckIds.TempFiles;
    public string Name => "Temporary Files";
    public DiagnosticCategory Category => DiagnosticCategory.Storage;

    public async Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        var scans = await _cleanupScanner.ScanAsync(cancellationToken);
        var tempBytes = scans
            .Where(s => s.Category is CleanupCategory.UserTemp or CleanupCategory.WindowsTemp)
            .Sum(s => s.SizeBytes);

        var gb = tempBytes / 1024.0 / 1024.0 / 1024.0;

        if (tempBytes > WarningThresholdBytes)
        {
            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.Warning,
                Severity = DiagnosticSeverity.Low,
                Summary = $"{gb:0.#} GB of temporary files have accumulated.",
                RecommendedAction = "Clean up temporary files to reclaim disk space.",
                FixAction = new DiagnosticFixAction
                {
                    Label = "Clean Up",
                    Kind = DiagnosticActionKind.Fix,
                    ConfirmationMessage = $"This removes {gb:0.#} GB of user and Windows temporary files. This can't be undone."
                }
            };
        }

        return new DiagnosticCheckResult
        {
            CheckId = Id,
            Name = Name,
            Category = Category,
            Status = DiagnosticStatus.Passed,
            Summary = $"Temporary files are at a normal level ({gb:0.#} GB)."
        };
    }
}
