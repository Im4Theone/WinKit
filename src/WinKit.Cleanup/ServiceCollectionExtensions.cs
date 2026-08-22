using Microsoft.Extensions.DependencyInjection;
using WinKit.Cleanup.Services;

namespace WinKit.Cleanup;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddWinKitCleanupTools(this IServiceCollection services)
    {
        services.AddSingleton<ICleanupScanner, CleanupScanner>();
        services.AddSingleton<ICleanupExecutor, CleanupExecutor>();
        return services;
    }
}
