using Microsoft.Extensions.DependencyInjection;
using WinKit.Diagnostics.Checks;
using WinKit.Diagnostics.Fixers;

namespace WinKit.Diagnostics;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddWinKitDiagnostics(this IServiceCollection services)
    {
        services.AddSingleton<IDiagnosticEngine, DiagnosticEngine>();
        services.AddSingleton<DiagnosticsSummaryStore>();
        services.AddSingleton<IDiagnosticsSummaryStore>(sp => sp.GetRequiredService<DiagnosticsSummaryStore>());

        services.AddSingleton<IDiagnosticCheck, WindowsIntegrityCheck>();
        services.AddSingleton<IDiagnosticCheck, WindowsUpdateCheck>();
        services.AddSingleton<IDiagnosticCheck, DriverCheck>();
        services.AddSingleton<IDiagnosticCheck, StartupCheck>();
        services.AddSingleton<IDiagnosticCheck, ServicesCheck>();
        services.AddSingleton<IDiagnosticCheck, DiskSpaceCheck>();
        services.AddSingleton<IDiagnosticCheck, TempFilesCheck>();
        services.AddSingleton<IDiagnosticCheck, DiskHealthCheck>();
        services.AddSingleton<IDiagnosticCheck, InternetConnectivityCheck>();
        services.AddSingleton<IDiagnosticCheck, DnsResolutionCheck>();
        services.AddSingleton<IDiagnosticCheck, NetworkAdapterCheck>();

        services.AddSingleton<IDiagnosticFixer, WindowsIntegrityFixer>();
        services.AddSingleton<IDiagnosticFixer, WindowsUpdateServiceFixer>();
        services.AddSingleton<IDiagnosticFixer, OpenDeviceManagerFixer>();
        services.AddSingleton<IDiagnosticFixer, TempFilesFixer>();
        services.AddSingleton<IDiagnosticFixer, DnsFlushFixer>();

        return services;
    }
}
