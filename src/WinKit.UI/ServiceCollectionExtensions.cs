using Microsoft.Extensions.DependencyInjection;
using WinKit.UI.Dialogs;
using WinKit.UI.Navigation;
using WinKit.UI.Theming;
using WinKit.UI.ViewModels;
using WinKit.UI.ViewModels.NetworkTools;
using WinKit.UI.ViewModels.SystemTools;

namespace WinKit.UI;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddWinKitUi(this IServiceCollection services)
    {
        services.AddSingleton<INavigationService, NavigationService>();
        services.AddSingleton<IDialogService, DialogService>();
        services.AddSingleton<ThemeApplier>();
        services.AddSingleton<NotificationHostViewModel>();

        services.AddTransient<ShellViewModel>();
        services.AddTransient<MainWindow>();

        services.AddTransient<OnboardingViewModel>();
        services.AddTransient<DashboardViewModel>();
        services.AddTransient<SettingsViewModel>();
        services.AddTransient<SystemToolsViewModel>();
        services.AddTransient<NetworkToolsViewModel>();
        services.AddTransient<CleanupViewModel>();
        services.AddTransient<DiagnosticsViewModel>();
        services.AddTransient<AboutViewModel>();

        services.AddTransient<ProcessManagerViewModel>();
        services.AddTransient<StartupManagerViewModel>();
        services.AddTransient<ServicesManagerViewModel>();
        services.AddTransient<InstalledAppsViewModel>();
        services.AddTransient<EnvironmentVariablesViewModel>();
        services.AddTransient<HostsFileViewModel>();
        services.AddTransient<SystemInformationViewModel>();

        services.AddTransient<PingViewModel>();
        services.AddTransient<DnsLookupViewModel>();
        services.AddTransient<TracerouteViewModel>();
        services.AddTransient<IpConfigurationViewModel>();
        services.AddTransient<NetworkMaintenanceViewModel>();

        return services;
    }
}
