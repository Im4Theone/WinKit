using System.Windows;
using System.Windows.Threading;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using WinKit.Cleanup;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;
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
    private static readonly TimeSpan UpdateCheckInterval = TimeSpan.FromHours(6);

    private IHost? _host;
    private DispatcherTimer? _updateCheckTimer;

    protected override async void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        DispatcherUnhandledException += OnDispatcherUnhandledException;
        AppDomain.CurrentDomain.UnhandledException += OnAppDomainUnhandledException;
        TaskScheduler.UnobservedTaskException += OnUnobservedTaskException;

        var builder = Host.CreateApplicationBuilder();
        builder.Services.AddWinKitInfrastructure();
        builder.Services.AddWinKitThemes();
        builder.Services.AddWinKitSystemTools();
        builder.Services.AddWinKitNetworkTools();
        builder.Services.AddWinKitCleanupTools();
        builder.Services.AddWinKitDiagnostics();
        builder.Services.AddWinKitUi();

        _host = builder.Build();
        await _host.StartAsync();

        var services = _host.Services;

        var profileService = services.GetRequiredService<IUserProfileService>();
        await profileService.LoadAsync();

        var settingsService = services.GetRequiredService<IAppSettingsService>();
        await settingsService.LoadAsync();

        var activityLogService = services.GetRequiredService<IActivityLogService>();
        await activityLogService.LoadAsync();

        var themeService = services.GetRequiredService<IThemeService>();
        await themeService.InitializeAsync();
        await themeService.ApplyThemeAsync(settingsService.ThemeName);

        services.GetRequiredService<ThemeApplier>().ApplyCurrent();

        var navigationService = services.GetRequiredService<INavigationService>();
        if (profileService.Current.HasCompletedOnboarding)
        {
            navigationService.NavigateTo<DashboardViewModel>();
        }
        else
        {
            navigationService.NavigateTo<OnboardingViewModel>();
        }

        var mainWindow = services.GetRequiredService<MainWindow>();
        MainWindow = mainWindow;

        if (settingsService.StartMinimized)
        {
            mainWindow.WindowState = WindowState.Minimized;
        }

        mainWindow.Show();

        if (settingsService.AutoCheckForUpdates)
        {
            _ = CheckForUpdatesInBackgroundAsync(services);

            _updateCheckTimer = new DispatcherTimer { Interval = UpdateCheckInterval };
            _updateCheckTimer.Tick += (_, _) => _ = CheckForUpdatesInBackgroundAsync(services);
            _updateCheckTimer.Start();
        }
    }

    private static async Task CheckForUpdatesInBackgroundAsync(IServiceProvider services)
    {
        try
        {
            var updateService = services.GetRequiredService<IUpdateService>();
            var settingsService = services.GetRequiredService<IAppSettingsService>();
            var notificationService = services.GetRequiredService<INotificationService>();
            var navigationService = services.GetRequiredService<INavigationService>();

            var result = await updateService.CheckForUpdateAsync();

            settingsService.LastUpdateCheckUtc = DateTimeOffset.UtcNow;
            await settingsService.SaveAsync();

            if (result.Status != UpdateCheckStatus.UpdateAvailable)
            {
                return;
            }

            if (string.Equals(result.LatestVersion, settingsService.SkippedUpdateVersion, StringComparison.OrdinalIgnoreCase))
            {
                return;
            }

            notificationService.Show(new NotificationRequest
            {
                Title = $"WinKit {result.LatestVersion} is available",
                Description = "Click to review what's changed and install it.",
                Severity = NotificationSeverity.Update,
                ActionText = "Update Now",
                Action = () =>
                {
                    var aboutViewModel = navigationService.NavigateTo<AboutViewModel>();
                    _ = aboutViewModel.ShowPendingUpdateAsync(result);
                },
                AutoDismissAfter = null
            });
        }
        catch (Exception)
        {
            // A background update check must never affect the running app.
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
        ReportUnhandledException(e.Exception, "Unhandled Exception");
        e.Handled = true;
    }

    private void OnAppDomainUnhandledException(object sender, UnhandledExceptionEventArgs e)
    {
        if (e.ExceptionObject is Exception exception)
        {
            ReportUnhandledException(exception, "Unhandled Exception (Background Thread)");
        }
    }

    private void OnUnobservedTaskException(object? sender, UnobservedTaskExceptionEventArgs e)
    {
        ReportUnhandledException(e.Exception, "Unobserved Task Exception");
        e.SetObserved();
    }

    private void ReportUnhandledException(Exception exception, string component)
    {
        try
        {
            AppPaths.EnsureCreated();
            System.IO.File.AppendAllText(
                System.IO.Path.Combine(AppPaths.LogsDirectory, "crash.log"),
                $"{DateTime.Now:O}{Environment.NewLine}{exception}{Environment.NewLine}{new string('-', 40)}{Environment.NewLine}");
        }
        catch (Exception)
        {
            // Best-effort logging; never let logging itself crash the handler.
        }

        // A crash handler must never throw itself, and background-thread/unobserved-task
        // exceptions can arrive with no live Dispatcher to marshal a dialog onto — both
        // the service resolution and the dialog are best-effort, wrapped defensively.
        try
        {
            var errorReportingService = _host?.Services.GetService<IErrorReportingService>();
            if (errorReportingService is null)
            {
                return;
            }

            var report = errorReportingService.CreateReport(exception, component);
            Dispatcher.Invoke(() =>
            {
                DialogWindow.ShowErrorReport(
                    MainWindow,
                    "Something went wrong",
                    "WinKit ran into an unexpected error. You can keep using the app, but some features may not work correctly until you restart.",
                    report,
                    errorReportingService.SendReportAsync);
            });
        }
        catch (Exception)
        {
            // The dialog itself failed to show; the crash log above is the fallback record.
        }
    }
}
