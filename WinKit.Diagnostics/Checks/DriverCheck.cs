using System.Management;
using System.Text;
using WinKit.Core.Models;

namespace WinKit.Diagnostics.Checks;

/// <summary>
/// Looks for devices Windows itself has flagged with a Device Manager error code
/// (Win32_PnPEntity.ConfigManagerErrorCode). Codes 22 (disabled) and 45 (not
/// currently connected) are reported as informational rather than a warning,
/// since both are normal, common states - not a driver problem.
/// </summary>
public sealed class DriverCheck : IDiagnosticCheck
{
    private static readonly Dictionary<int, string> KnownCodes = new()
    {
        [1] = "device is not configured correctly",
        [3] = "driver may be corrupted, or the system is low on resources",
        [10] = "device cannot start",
        [18] = "drivers need to be reinstalled",
        [22] = "device is disabled",
        [28] = "drivers are not installed",
        [31] = "device is not working properly",
        [37] = "driver returned a failure",
        [39] = "driver may be corrupted or missing",
        [43] = "device reported a failure and was stopped by Windows",
        [45] = "device is not currently connected",
    };

    public string Id => DiagnosticCheckIds.Drivers;
    public string Name => "Drivers";
    public DiagnosticCategory Category => DiagnosticCategory.Drivers;

    public Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        return Task.Run(() =>
        {
            var benign = new List<string>();
            var problems = new List<string>();

            try
            {
                using var searcher = new ManagementObjectSearcher(
                    "SELECT Name, ConfigManagerErrorCode FROM Win32_PnPEntity WHERE ConfigManagerErrorCode <> 0");

                foreach (ManagementObject device in searcher.Get())
                {
                    cancellationToken.ThrowIfCancellationRequested();

                    var name = device["Name"]?.ToString() ?? "Unknown device";
                    var code = device["ConfigManagerErrorCode"] is not null
                        ? Convert.ToInt32(device["ConfigManagerErrorCode"])
                        : 0;

                    var description = KnownCodes.GetValueOrDefault(code, $"reporting configuration error code {code}");
                    var line = $"{name} - {description} (code {code})";

                    // Code 22 (user-disabled) and 45 (not currently connected - e.g. a USB
                    // drive or dock that was plugged in once and isn't now) are Windows
                    // keeping a "ghost" PnP entry around; neither indicates an actual
                    // driver problem, so both are informational rather than a warning.
                    if (code is 22 or 45)
                    {
                        benign.Add(line);
                    }
                    else
                    {
                        problems.Add(line);
                    }
                }
            }
            catch (ManagementException ex)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Informational,
                    Summary = "Could not query device status via WMI.",
                    TechnicalDetail = ex.Message
                };
            }

            if (problems.Count > 0)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Warning,
                    Severity = DiagnosticSeverity.Medium,
                    Summary = problems.Count == 1
                        ? "1 device is reporting a configuration problem."
                        : $"{problems.Count} devices are reporting configuration problems.",
                    TechnicalDetail = BuildDetail(problems, benign),
                    RecommendedAction = "Open Device Manager to review the affected device(s) and update or reinstall their drivers.",
                    FixAction = new DiagnosticFixAction
                    {
                        Label = "Open Device Manager",
                        Kind = DiagnosticActionKind.Fix,
                        ConfirmationMessage = "This opens the built-in Windows Device Manager so you can review the affected devices."
                    }
                };
            }

            if (benign.Count > 0)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Informational,
                    Summary = $"{benign.Count} device(s) are disabled or not currently connected.",
                    TechnicalDetail = string.Join(Environment.NewLine, benign)
                };
            }

            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.Passed,
                Summary = "All devices are working correctly."
            };
        }, cancellationToken);
    }

    private static string BuildDetail(List<string> problems, List<string> benign)
    {
        var sb = new StringBuilder();
        foreach (var line in problems)
        {
            sb.AppendLine(line);
        }

        if (benign.Count > 0)
        {
            sb.AppendLine($"Also disabled or not connected: {benign.Count} device(s).");
        }

        return sb.ToString().TrimEnd();
    }
}
