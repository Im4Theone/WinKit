using Microsoft.Extensions.DependencyInjection;
using WinKit.SystemTools.Services;

namespace WinKit.SystemTools;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddWinKitSystemTools(this IServiceCollection services)
    {
        services.AddSingleton<ISystemInfoService, SystemInfoService>();
        services.AddSingleton<IProcessManagerService, ProcessManagerService>();
        services.AddSingleton<IStartupManagerService, StartupManagerService>();
        services.AddSingleton<IServicesManagerService, ServicesManagerService>();
        services.AddSingleton<IInstalledAppsService, InstalledAppsService>();
        services.AddSingleton<IEnvironmentVariablesService, EnvironmentVariablesService>();
        services.AddSingleton<IHostsFileService, HostsFileService>();
        return services;
    }
}
