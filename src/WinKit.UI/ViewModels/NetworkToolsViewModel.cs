using WinKit.UI.Navigation;
using WinKit.UI.ViewModels.NetworkTools;

namespace WinKit.UI.ViewModels;

public sealed class NetworkToolsViewModel : ToolPageViewModelBase, IAsyncInitializable
{
    public NetworkToolsViewModel(
        PingViewModel ping,
        DnsLookupViewModel dnsLookup,
        TracerouteViewModel traceroute,
        IpConfigurationViewModel ipConfiguration,
        NetworkMaintenanceViewModel maintenance)
        : base(new ToolTabItem[]
        {
            new("Ping", ping),
            new("DNS Lookup", dnsLookup),
            new("Traceroute", traceroute),
            new("IP Configuration", ipConfiguration, () => ipConfiguration.RefreshCommand.ExecuteAsync(null)),
            new("Maintenance", maintenance),
        })
    {
    }

    public Task InitializeAsync()
    {
        SelectFirstTab();
        return Task.CompletedTask;
    }
}
