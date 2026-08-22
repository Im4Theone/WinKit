using System.Net.NetworkInformation;
using System.ServiceProcess;
using WinKit.Cleanup.Models;
using WinKit.Cleanup.Services;
using WinKit.Core.Models;

namespace WinKit.Diagnostics;

public sealed class DiagnosticsRunner : IDiagnosticsRunner
{
    private const long TempFilesWarningThresholdBytes = 2L * 1024 * 1024 * 1024; // 2 GB

    private readonly ICleanupScanner _cleanupScanner;

    public DiagnosticsRunner(ICleanupScanner cleanupScanner)
    {
        _cleanupScanner = cleanupScanner;
    }

    public async Task<IReadOnlyList<DiagnosticCheckResult>> RunAllAsync(
        IProgress<DiagnosticCheckResult>? progress = null,
        CancellationToken cancellationToken = default)
    {
        var results = new List<DiagnosticCheckResult>();

        async Task RunCheck(Func<CancellationToken, Task<DiagnosticCheckResult>> check)
        {
            var result = await check(cancellationToken);
            results.Add(result);
            progress?.Report(result);
        }

        await RunCheck(CheckInternetConnectivityAsync);
        await RunCheck(CheckDnsResolutionAsync);
        await RunCheck(CheckDefaultGatewayAsync);
        await RunCheck(CheckWindowsFirewallAsync);
        await RunCheck(CheckWindowsUpdateAsync);
        await RunCheck(CheckTemporaryFilesAsync);

        return results;
    }

    private static async Task<DiagnosticCheckResult> CheckInternetConnectivityAsync(CancellationToken ct)
    {
        return await PingCheck("Internet connectivity", "1.1.1.1",
            "Your computer can reach the internet.",
            "Your computer could not reach the internet.",
            "Check your Wi-Fi or ethernet connection, or contact your network administrator.");
    }

    private static async Task<DiagnosticCheckResult> CheckDnsResolutionAsync(CancellationToken ct)
    {
        try
        {
            var entry = await System.Net.Dns.GetHostEntryAsync("www.microsoft.com", ct);
            return new DiagnosticCheckResult
            {
                Name = "DNS resolution",
                Status = entry.AddressList.Length > 0 ? DiagnosticStatus.Ok : DiagnosticStatus.Warning,
                Summary = entry.AddressList.Length > 0
                    ? "DNS lookups are working."
                    : "DNS returned no results."
            };
        }
        catch (System.Net.Sockets.SocketException ex)
        {
            return new DiagnosticCheckResult
            {
                Name = "DNS resolution",
                Status = DiagnosticStatus.Error,
                Summary = "DNS lookups are failing.",
                Explanation = ex.Message,
                RecommendedAction = "Try flushing the DNS cache from Tools > Network, or check your DNS server settings."
            };
        }
    }

    private static async Task<DiagnosticCheckResult> CheckDefaultGatewayAsync(CancellationToken ct)
    {
        var gateway = NetworkInterface.GetAllNetworkInterfaces()
            .Where(nic => nic.OperationalStatus == OperationalStatus.Up)
            .SelectMany(nic => nic.GetIPProperties().GatewayAddresses)
            .Select(g => g.Address)
            .FirstOrDefault(a => a.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork);

        if (gateway is null)
        {
            return new DiagnosticCheckResult
            {
                Name = "Default gateway",
                Status = DiagnosticStatus.Warning,
                Summary = "No default gateway was found.",
                RecommendedAction = "Check that your network adapter is connected."
            };
        }

        return await PingCheck("Default gateway", gateway.ToString(),
            "Your router is responding.",
            "Your router did not respond to a ping.",
            "This can be normal if your router blocks ping. If you're also offline, restart your router.");
    }

    private static Task<DiagnosticCheckResult> CheckWindowsFirewallAsync(CancellationToken ct)
    {
        return Task.Run(() =>
        {
            try
            {
                using var service = new ServiceController("MpsSvc");
                var running = service.Status == ServiceControllerStatus.Running;
                return new DiagnosticCheckResult
                {
                    Name = "Windows Firewall",
                    Status = running ? DiagnosticStatus.Ok : DiagnosticStatus.Warning,
                    Summary = running ? "Windows Firewall is active." : "Windows Firewall service is not running.",
                    RecommendedAction = running ? null : "Open Windows Security to review your firewall settings."
                };
            }
            catch (InvalidOperationException)
            {
                return new DiagnosticCheckResult
                {
                    Name = "Windows Firewall",
                    Status = DiagnosticStatus.Warning,
                    Summary = "Could not determine Windows Firewall status."
                };
            }
        }, ct);
    }

    private static Task<DiagnosticCheckResult> CheckWindowsUpdateAsync(CancellationToken ct)
    {
        return Task.Run(() =>
        {
            try
            {
                using var service = new ServiceController("wuauserv");
                var disabled = service.StartType == ServiceStartMode.Disabled;
                return new DiagnosticCheckResult
                {
                    Name = "Windows Update",
                    Status = disabled ? DiagnosticStatus.Warning : DiagnosticStatus.Ok,
                    Summary = disabled
                        ? "The Windows Update service is disabled."
                        : "The Windows Update service is available.",
                    RecommendedAction = disabled ? "Enable the Windows Update service in Services.msc." : null
                };
            }
            catch (InvalidOperationException)
            {
                return new DiagnosticCheckResult
                {
                    Name = "Windows Update",
                    Status = DiagnosticStatus.Warning,
                    Summary = "Could not determine Windows Update status."
                };
            }
        }, ct);
    }

    private async Task<DiagnosticCheckResult> CheckTemporaryFilesAsync(CancellationToken ct)
    {
        var scans = await _cleanupScanner.ScanAsync(ct);
        var tempBytes = scans
            .Where(s => s.Category is CleanupCategory.UserTemp or CleanupCategory.WindowsTemp)
            .Sum(s => s.SizeBytes);

        var warning = tempBytes > TempFilesWarningThresholdBytes;
        var gb = tempBytes / 1024.0 / 1024.0 / 1024.0;

        return new DiagnosticCheckResult
        {
            Name = "Temporary files",
            Status = warning ? DiagnosticStatus.Warning : DiagnosticStatus.Ok,
            Summary = warning
                ? $"{gb:0.0} GB of temporary files have accumulated."
                : $"Temporary files are at a normal level ({gb:0.0} GB).",
            RecommendedAction = warning ? "Run Clean Temporary Files from Tools > Cleanup." : null
        };
    }

    private static async Task<DiagnosticCheckResult> PingCheck(
        string name, string target, string okSummary, string failSummary, string recommendedAction)
    {
        try
        {
            using var ping = new Ping();
            var reply = await ping.SendPingAsync(target, 3000);
            var ok = reply.Status == IPStatus.Success;
            return new DiagnosticCheckResult
            {
                Name = name,
                Status = ok ? DiagnosticStatus.Ok : DiagnosticStatus.Error,
                Summary = ok ? okSummary : failSummary,
                RecommendedAction = ok ? null : recommendedAction
            };
        }
        catch (PingException ex)
        {
            return new DiagnosticCheckResult
            {
                Name = name,
                Status = DiagnosticStatus.Error,
                Summary = failSummary,
                Explanation = ex.Message,
                RecommendedAction = recommendedAction
            };
        }
    }
}
