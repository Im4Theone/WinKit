using WinKit.Core.Models;

namespace WinKit.Diagnostics.Checks;

/// <summary>
/// Reports free space pressure on the system drive. Deliberately framed as a
/// capacity/maintenance signal, never as a hardware failure - that's DiskHealthCheck's job.
/// </summary>
public sealed class DiskSpaceCheck : IDiagnosticCheck
{
    // Absolute free space, not percentage: a 4TB drive at 12% free still has ~480GB
    // free, which is not "low on space" by any practical measure. Percentage alone
    // produces false positives on large drives, so gigabytes is the primary signal.
    private const double CriticalFreeGb = 10.0;
    private const double WarningFreeGb = 25.0;

    public string Id => DiagnosticCheckIds.DiskSpace;
    public string Name => "Disk Space";
    public DiagnosticCategory Category => DiagnosticCategory.Storage;

    public Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        return Task.Run(() =>
        {
            var systemRoot = Path.GetPathRoot(Environment.SystemDirectory) ?? @"C:\";
            var drive = new DriveInfo(systemRoot);

            if (!drive.IsReady)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Informational,
                    Summary = $"Could not read free space for drive {systemRoot}."
                };
            }

            var freePercent = drive.TotalSize == 0 ? 100.0 : drive.AvailableFreeSpace * 100.0 / drive.TotalSize;
            var freeGb = drive.AvailableFreeSpace / 1024.0 / 1024.0 / 1024.0;
            var totalGb = drive.TotalSize / 1024.0 / 1024.0 / 1024.0;
            var detail = $"{systemRoot} - {freeGb:0.#} GB free of {totalGb:0.#} GB ({freePercent:0.#}% free).";

            if (freeGb < CriticalFreeGb)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Failed,
                    Severity = DiagnosticSeverity.High,
                    Summary = $"The system drive ({systemRoot}) is critically low on space.",
                    TechnicalDetail = detail,
                    RecommendedAction = "Free up disk space before it affects Windows Update, app installs, or general stability.",
                    FixAction = new DiagnosticFixAction
                    {
                        Label = "Open Cleanup",
                        Kind = DiagnosticActionKind.Review,
                        ConfirmationMessage = "This opens the Cleanup tool so you can review and remove reclaimable files."
                    }
                };
            }

            if (freeGb < WarningFreeGb)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Warning,
                    Severity = DiagnosticSeverity.Medium,
                    Summary = $"The system drive ({systemRoot}) is running low on space.",
                    TechnicalDetail = detail,
                    RecommendedAction = "Consider freeing up disk space soon.",
                    FixAction = new DiagnosticFixAction
                    {
                        Label = "Open Cleanup",
                        Kind = DiagnosticActionKind.Review,
                        ConfirmationMessage = "This opens the Cleanup tool so you can review and remove reclaimable files."
                    }
                };
            }

            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.Passed,
                Summary = $"The system drive has {freeGb:0.#} GB free ({freePercent:0.#}%).",
                TechnicalDetail = detail
            };
        }, cancellationToken);
    }
}
