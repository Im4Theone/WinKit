using Microsoft.Extensions.DependencyInjection;

namespace WinKit.Diagnostics;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddWinKitDiagnostics(this IServiceCollection services)
    {
        services.AddSingleton<IDiagnosticsRunner, DiagnosticsRunner>();
        return services;
    }
}
