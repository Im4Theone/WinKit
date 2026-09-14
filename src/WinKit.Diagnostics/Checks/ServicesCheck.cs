using System.Management;
using System.ServiceProcess;
using WinKit.Core.Models;

namespace WinKit.Diagnostics.Checks;

/// <summary>
/// Checks a small, deliberately conservative baseline of services a healthy
/// desktop needs running. Windows Firewall and Windows Defender are only flagged
/// if no third-party product (via the SecurityCenter2 WMI namespace) is covering
/// for them - otherwise a normally-configured third-party security suite would
/// show up as a false positive. This never changes a service; it only reports.
/// </summary>
public sealed class ServicesCheck : IDiagnosticCheck
{
    private sealed record BaselineService(string ServiceName, string DisplayName, string ImpactIfStopped);

    private static readonly BaselineService[] Baseline =
    {
        new("BFE", "Base Filtering Engine",
            "Windows Firewall and other network filtering will stop working."),
        new("Dnscache", "DNS Client",
            "Name resolution may become slow or unreliable for most apps."),
        new("Dhcp", "DHCP Client",
            "Automatic IP address configuration will not work."),
    };

    public string Id => DiagnosticCheckIds.Services;
    public string Name => "Services";
    public DiagnosticCategory Category => DiagnosticCategory.Services;

    public Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        return Task.Run(() =>
        {
            var problems = new List<string>();
            var criticalDown = false;

            foreach (var service in Baseline)
            {
                if (!TryGetStatus(service.ServiceName, out var status) || status == ServiceControllerStatus.Running)
                {
                    continue;
                }

                problems.Add($"{service.DisplayName} ({service.ServiceName}) is {status}. {service.ImpactIfStopped}");
                criticalDown = true;
            }

            if (!IsFirewallCovered())
            {
                problems.Add("Windows Firewall is stopped and no third-party firewall product was detected.");
                criticalDown = true;
            }

            if (!IsAntivirusCovered())
            {
                problems.Add("Windows Defender is stopped and no third-party antivirus product was detected.");
            }

            if (problems.Count == 0)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Passed,
                    Summary = "Core Windows services are running normally."
                };
            }

            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = criticalDown ? DiagnosticStatus.Failed : DiagnosticStatus.Warning,
                Severity = criticalDown ? DiagnosticSeverity.High : DiagnosticSeverity.Medium,
                Summary = problems.Count == 1
                    ? "1 important service needs attention."
                    : $"{problems.Count} important services need attention.",
                TechnicalDetail = string.Join(Environment.NewLine, problems),
                RecommendedAction = "Open Services in System Tools to review and start the affected service(s).",
                FixAction = new DiagnosticFixAction
                {
                    Label = "Review Services",
                    Kind = DiagnosticActionKind.Review,
                    ConfirmationMessage = "This opens the Services tool, where you can start the affected service(s) yourself."
                }
            };
        }, cancellationToken);
    }

    private static bool TryGetStatus(string serviceName, out ServiceControllerStatus status)
    {
        try
        {
            using var controller = new ServiceController(serviceName);
            status = controller.Status;
            return true;
        }
        catch (InvalidOperationException)
        {
            status = default;
            return false;
        }
    }

    private static bool IsFirewallCovered()
    {
        if (TryGetStatus("MpsSvc", out var status) && status == ServiceControllerStatus.Running)
        {
            return true;
        }

        return HasSecurityCenterProduct("FirewallProduct");
    }

    private static bool IsAntivirusCovered()
    {
        if (TryGetStatus("WinDefend", out var status) && status == ServiceControllerStatus.Running)
        {
            return true;
        }

        return HasSecurityCenterProduct("AntiVirusProduct");
    }

    private static bool HasSecurityCenterProduct(string className)
    {
        try
        {
            var scope = new ManagementScope(@"root\SecurityCenter2");
            using var searcher = new ManagementObjectSearcher(scope, new ObjectQuery($"SELECT * FROM {className}"));
            using var results = searcher.Get();
            return results.Count > 0;
        }
        catch (ManagementException)
        {
            // SecurityCenter2 isn't available on this SKU (e.g. Server); don't penalize for it.
            return true;
        }
        catch (UnauthorizedAccessException)
        {
            return true;
        }
    }
}
