using Microsoft.Extensions.DependencyInjection;
using WinKit.Network.Services;

namespace WinKit.Network;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddWinKitNetworkTools(this IServiceCollection services)
    {
        services.AddSingleton<IPingService, PingService>();
        services.AddSingleton<IDnsLookupService, DnsLookupService>();
        services.AddSingleton<ITracerouteService, TracerouteService>();
        services.AddSingleton<IIpConfigurationService, IpConfigurationService>();
        services.AddSingleton<INetworkMaintenanceService, NetworkMaintenanceService>();
        return services;
    }
}
