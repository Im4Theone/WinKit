using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;
using WinKit.Diagnostics;
using WinKit.SystemTools.Models;
using WinKit.SystemTools.Services;
using WinKit.UI.Formatting;
using WinKit.UI.Navigation;

namespace WinKit.UI.ViewModels;

public sealed partial class DashboardViewModel : ObservableObject, IAsyncInitializable
{
    private const int RecentActivityLimit = 5;

    private readonly IUserProfileService _profileService;
    private readonly ISystemInfoService _systemInfoService;
    private readonly IDiagnosticsRunner _diagnosticsRunner;
    private readonly INavigationService _navigationService;
    private readonly ReadOnlyObservableCollection<ActivityEntry> _activityEntries;

    public DashboardViewModel(
        IUserProfileService profileService,
        ISystemInfoService systemInfoService,
        IDiagnosticsRunner diagnosticsRunner,
        IActivityLogService activityLogService,
        INavigationService navigationService)
    {
        _profileService = profileService;
        _systemInfoService = systemInfoService;
        _diagnosticsRunner = diagnosticsRunner;
        _navigationService = navigationService;
        _activityEntries = activityLogService.Entries;
        ((INotifyCollectionChanged)_activityEntries).CollectionChanged += (_, _) => RefreshRecentActivity();
        RefreshRecentActivity();

        UpdateGreeting();

        var refreshTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(5) };
        refreshTimer.Tick += async (_, _) =>
        {
            UpdateGreeting();
            await RefreshOverviewAsync();
        };
        refreshTimer.Start();

        var uptimeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        uptimeTimer.Tick += (_, _) => UptimeText = UptimeFormatter.Format(TimeSpan.FromMilliseconds(Environment.TickCount64));
        uptimeTimer.Start();
    }

    public ObservableCollection<ActivityEntry> RecentActivityEntries { get; } = new();

    [ObservableProperty]
    private bool _hasMoreActivity;

    [ObservableProperty]
    private string _greetingTitle = string.Empty;

    [ObservableProperty]
    private string _statusMessage = "Checking your system...";

    [ObservableProperty]
    private bool _statusIsHealthy = true;

    [ObservableProperty]
    private SystemOverview? _overview;

    [ObservableProperty]
    private bool _isLoading = true;

    public StorageVolumeInfo? PrimaryStorage => Overview?.Storage.FirstOrDefault();

    [ObservableProperty]
    private string _uptimeText = UptimeFormatter.Format(TimeSpan.FromMilliseconds(Environment.TickCount64));

    partial void OnOverviewChanged(SystemOverview? value)
    {
        OnPropertyChanged(nameof(PrimaryStorage));
    }

    public async Task InitializeAsync()
    {
        await RefreshOverviewAsync();
        await RunHealthCheckAsync();
        IsLoading = false;
    }

    private async Task RefreshOverviewAsync()
    {
        Overview = await _systemInfoService.GetOverviewAsync();
    }

    private async Task RunHealthCheckAsync()
    {
        var results = await _diagnosticsRunner.RunAllAsync();
        var issues = results.Count(r => r.Status != DiagnosticStatus.Ok);
        StatusIsHealthy = issues == 0;
        StatusMessage = issues switch
        {
            0 => "Your system is running normally.",
            1 => "1 issue needs your attention.",
            _ => $"{issues} issues need your attention."
        };
    }

    private void UpdateGreeting()
    {
        var name = _profileService.Current.Name;
        var hour = DateTime.Now.Hour;
        var period = hour switch
        {
            >= 5 and < 12 => "Good morning",
            >= 12 and < 18 => "Good afternoon",
            >= 18 and < 23 => "Good evening",
            _ => "Hello, night owl"
        };
        GreetingTitle = string.IsNullOrWhiteSpace(name) ? $"{period}!" : $"{period}, {name}";
    }

    private void RefreshRecentActivity()
    {
        RecentActivityEntries.Clear();
        foreach (var entry in _activityEntries.Take(RecentActivityLimit))
        {
            RecentActivityEntries.Add(entry);
        }

        HasMoreActivity = _activityEntries.Count > RecentActivityLimit;
    }

    [RelayCommand]
    private void ShowMoreActivity() => _navigationService.NavigateTo<ActivityViewModel>();

    [RelayCommand]
    private void RunDiagnostics() => _navigationService.NavigateTo<DiagnosticsViewModel>();

    [RelayCommand]
    private void CleanTemporaryFiles() => _navigationService.NavigateTo<CleanupViewModel>();

    [RelayCommand]
    private void NetworkTest() => _navigationService.NavigateTo<NetworkToolsViewModel>();

    [RelayCommand]
    private void SystemInformation() => _navigationService.NavigateTo<SystemToolsViewModel>();
}
