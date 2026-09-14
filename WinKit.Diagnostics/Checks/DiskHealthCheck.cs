using System.Management;
using WinKit.Core.Models;

namespace WinKit.Diagnostics.Checks;

/// <summary>
/// Reads the SMART failure-prediction flag Windows storage drivers expose via WMI.
/// Not every controller/driver supports this (common on NVMe and virtual machines),
/// so an unsupported query is reported as NotApplicable rather than a false Warning.
/// </summary>
public sealed class DiskHealthCheck : IDiagnosticCheck
{
    public string Id => DiagnosticCheckIds.DiskHealth;
    public string Name => "Disk Health";
    public DiagnosticCategory Category => DiagnosticCategory.Storage;

    public Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        return Task.Run(() =>
        {
            try
            {
                var scope = new ManagementScope(@"root\wmi");
                using var searcher = new ManagementObjectSearcher(
                    scope, new ObjectQuery("SELECT InstanceName, PredictFailure FROM MSStorageDriver_FailurePredictStatus"));
                using var results = searcher.Get();

                if (results.Count == 0)
                {
                    return NotApplicable("No drives reported SMART failure-prediction status.");
                }

                var failing = new List<string>();
                foreach (ManagementObject drive in results)
                {
                    var predictFailure = drive["PredictFailure"] is bool b && b;
                    if (predictFailure)
                    {
                        failing.Add(drive["InstanceName"]?.ToString() ?? "Unknown drive");
                    }
                }

                if (failing.Count > 0)
                {
                    return new DiagnosticCheckResult
                    {
                        CheckId = Id,
                        Name = Name,
                        Category = Category,
                        Status = DiagnosticStatus.Failed,
                        Severity = DiagnosticSeverity.High,
                        Summary = "A drive is reporting a SMART failure-prediction warning.",
                        TechnicalDetail = string.Join(Environment.NewLine, failing),
                        RecommendedAction = "Back up your data immediately and plan to replace this drive. " +
                                            "WinKit cannot repair failing hardware automatically."
                    };
                }

                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Passed,
                    Summary = "No SMART failure prediction detected."
                };
            }
            catch (ManagementException ex)
            {
                return NotApplicable($"SMART failure prediction isn't available on this system: {ex.Message}");
            }
            catch (UnauthorizedAccessException ex)
            {
                return NotApplicable($"SMART failure prediction isn't accessible on this system: {ex.Message}");
            }
        }, cancellationToken);
    }

    private DiagnosticCheckResult NotApplicable(string detail) => new()
    {
        CheckId = Id,
        Name = Name,
        Category = Category,
        Status = DiagnosticStatus.NotApplicable,
        Summary = "SMART failure prediction isn't available for this drive/controller.",
        TechnicalDetail = detail
    };
}
