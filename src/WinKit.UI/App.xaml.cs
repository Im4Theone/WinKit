using System.Windows;
using System.Windows.Threading;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using WinKit.Cleanup;
using WinKit.Core.Abstractions;
using WinKit.Diagnostics;
using WinKit.Infrastructure;
using WinKit.Network;
using WinKit.SystemTools;
using WinKit.Themes;
using WinKit.UI.Dialogs;
using WinKit.UI.Navigation;
using WinKit.UI.Theming;
using WinKit.UI.ViewModels;

namespace WinKit.UI;

public partial class App : Application
{
    private IHost? _host;

    protected override async void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        DispatcherUnhandledException += OnDispatcherUnhandledException;
        Trace("OnStartup begin");

        var builder = Host.CreateApplicationBuilder();
        builder.Services.AddWinKitInfrastructure();
        builder.Services.AddWinKitThemes();
        builder.Services.AddWinKitSystemTools();
        builder.Services.AddWinKitNetworkTools();
        builder.Services.AddWinKitCleanupTools();
        builder.Services.AddWinKitDiagnostics();
        builder.Services.AddWinKitUi();
        Trace("services registered");

        _host = builder.Build();
        Trace("host built");
        await _host.StartAsync();
        Trace("host started");

        var services = _host.Services;

        var profileService = services.GetRequiredService<IUserProfileService>();
        await profileService.LoadAsync();
        Trace("profile loaded");

        var settingsService = services.GetRequiredService<IAppSettingsService>();
        await settingsService.LoadAsync();
        Trace("settings loaded");

        var themeService = services.GetRequiredService<IThemeService>();
        await themeService.InitializeAsync();
        Trace("themes initialized");
        await themeService.ApplyThemeAsync(settingsService.ThemeName);
        Trace("theme applied: " + settingsService.ThemeName);

        services.GetRequiredService<ThemeApplier>().ApplyCurrent();
        Trace("theme applier applied");

        var navigationService = services.GetRequiredService<INavigationService>();
        if (profileService.Current.HasCompletedOnboarding)
        {
            navigationService.NavigateTo<DashboardViewModel>();
            Trace("navigated to dashboard");
        }
        else
        {
            navigationService.NavigateTo<OnboardingViewModel>();
            Trace("navigated to onboarding");
        }

        var mainWindow = services.GetRequiredService<MainWindow>();
        Trace("main window constructed");
        MainWindow = mainWindow;

        if (settingsService.StartMinimized)
        {
            mainWindow.WindowState = WindowState.Minimized;
        }

        mainWindow.Show();
        Trace("main window shown");
    }

    private static void Trace(string step)
    {
        try
        {
            AppPaths.EnsureCreated();
            System.IO.File.AppendAllText(
                System.IO.Path.Combine(AppPaths.LogsDirectory, "startup-trace.log"),
                $"{DateTime.Now:O} {step}{Environment.NewLine}");
        }
        catch (Exception)
        {
            // Best-effort tracing only.
        }
    }

    protected override async void OnExit(ExitEventArgs e)
    {
        if (_host is not null)
        {
            await _host.StopAsync();
            _host.Dispose();
        }

        base.OnExit(e);
    }

    private void OnDispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        try
        {
            AppPaths.EnsureCreated();
            System.IO.File.AppendAllText(
                System.IO.Path.Combine(AppPaths.LogsDirectory, "crash.log"),
                $"{DateTime.Now:O}{Environment.NewLine}{e.Exception}{Environment.NewLine}{new string('-', 40)}{Environment.NewLine}");
        }
        catch (Exception)
        {
            // Best-effort logging; never let logging itself crash the handler.
        }

        try
        {
            DialogWindow.ShowError(
                MainWindow,
                "Something went wrong",
                "WinKit ran into an unexpected error. You can keep using the app, but some features may not work correctly until you restart.",
                e.Exception.ToString());
        }
        catch (Exception)
        {
            // The dialog itself failed to show; the crash log above is the fallback record.
        }

        e.Handled = true;
    }
}
