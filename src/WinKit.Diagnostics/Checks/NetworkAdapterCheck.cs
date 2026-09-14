using WinKit.Core.Models;
using WinKit.Network.Services;

namespace WinKit.Diagnostics.Checks;

/// <summary>Reuses the IP Configuration tool's adapter enumeration instead of duplicating it.</summary>
public sealed class NetworkAdapterCheck : IDiagnosticCheck
{
    private readonly IIpConfigurationService _ipConfigurationService;

    public NetworkAdapterCheck(IIpConfigurationService ipConfigurationService)
    {
        _ipConfigurationService = ipConfigurationService;
    }

    public string Id => DiagnosticCheckIds.NetworkAdapter;
    public string Name => "Network Adapter";
    public DiagnosticCategory Category => DiagnosticCategory.Network;

    public async Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        var adapters = await _ipConfigurationService.GetAdaptersAsync(cancellationToken);
        var active = adapters.Where(a => a.IsUp).ToList();

        if (active.Count == 0)
        {
            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.Failed,
                Severity = DiagnosticSeverity.High,
                Summary = "No active network adapter was found.",
                RecommendedAction = "Check that Wi-Fi or ethernet is enabled and connected."
            };
        }

        var withoutGateway = active.Where(a => a.Gateways.Count == 0).ToList();
        if (withoutGateway.Count == active.Count)
        {
            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.Warning,
                Severity = DiagnosticSeverity.Medium,
                Summary = "The active network adapter has no default gateway configured.",
                TechnicalDetail = string.Join(Environment.NewLine, active.Select(Describe)),
                RecommendedAction = "Check your IP configuration in Network Tools > IP Configuration, or reconnect to your network."
            };
        }

        var primary = active.First(a => a.Gateways.Count > 0);
        return new DiagnosticCheckResult
        {
            CheckId = Id,
            Name = Name,
            Category = Category,
            Status = DiagnosticStatus.Passed,
            Summary = $"{primary.Name} is connected with a valid IP configuration.",
            TechnicalDetail = Describe(primary)
        };
    }

    private static string Describe(WinKit.Network.Models.AdapterConfiguration adapter) =>
        $"{adapter.Name}: IPv4 {string.Join(", ", adapter.IPv4Addresses)}; " +
        $"Gateway {string.Join(", ", adapter.Gateways)}; DNS {string.Join(", ", adapter.DnsServers)}";
}
