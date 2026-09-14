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
    private readonly IDiagnosticsSummaryStore _diagnosticsSummaryStore;
    private readonly INavigationService _navigationService;
    private readonly ReadOnlyObservableCollection<ActivityEntry> _activityEntries;
    private readonly DispatcherTimer _refreshTimer;
    private readonly DispatcherTimer _uptimeTimer;

    public DashboardViewModel(
        IUserProfileService profileService,
        ISystemInfoService systemInfoService,
        IDiagnosticsSummaryStore diagnosticsSummaryStore,
        IActivityLogService activityLogService,
        INavigationService navigationService)
    {
        _profileService = profileService;
        _systemInfoService = systemInfoService;
        _diagnosticsSummaryStore = diagnosticsSummaryStore;
        _navigationService = navigationService;
        _activityEntries = activityLogService.Entries;
        ((INotifyCollectionChanged)_activityEntries).CollectionChanged += (_, _) => RefreshRecentActivity();
        RefreshRecentActivity();

        _diagnosticsSummaryStore.Changed += (_, _) => UpdateDiagnosticsStatus();
        UpdateDiagnosticsStatus();

        UpdateGreeting();

        // Both timers only run while the Dashboard is the visible page (see UpdateTimerState) -
        // otherwise the 5-second tick would keep issuing WMI queries indefinitely in the
        // background for a page nobody is looking at.
        _refreshTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(5) };
        _refreshTimer.Tick += async (_, _) =>
        {
            UpdateGreeting();
            await RefreshOverviewAsync();
        };

        _uptimeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        _uptimeTimer.Tick += (_, _) => UptimeText = UptimeFormatter.Format(TimeSpan.FromMilliseconds(Environment.TickCount64));

        _navigationService.Navigated += (_, _) => UpdateTimerState();
        UpdateTimerState();
    }

    private void UpdateTimerState()
    {
        if (_navigationService.CurrentViewModel == this)
        {
            _refreshTimer.Start();
            _uptimeTimer.Start();
        }
        else
        {
            _refreshTimer.Stop();
            _uptimeTimer.Stop();
        }
    }

    public ObservableCollection<ActivityEntry> RecentActivityEntries { get; } = new();

    [ObservableProperty]
    private bool _hasMoreActivity;

    [ObservableProperty]
    private string _greetingTitle = string.Empty;

    [ObservableProperty]
    private string _statusMessage = "Diagnostics haven't been run yet.";

    /// <summary>Null = diagnostics haven't run this session yet (shown as a neutral state, not "healthy").</summary>
    [ObservableProperty]
    private bool? _statusIsHealthy;

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
        IsLoading = false;
    }

    private async Task RefreshOverviewAsync()
    {
        Overview = await _systemInfoService.GetOverviewAsync();
    }

    /// <summary>
    /// Reflects the last completed Diagnostics scan, if any. Never triggers a scan itself -
    /// that only happens when the user opens the Diagnostics page or runs a fix from it.
    /// </summary>
    private void UpdateDiagnosticsStatus()
    {
        var summary = _diagnosticsSummaryStore.LastSummary;
        if (summary is null)
        {
            StatusIsHealthy = null;
            StatusMessage = "Diagnostics haven't been run yet.";
            return;
        }

        StatusIsHealthy = summary.IssueCount == 0;
        StatusMessage = summary.IssueCount switch
        {
            0 => "Your system is running normally.",
            1 => "1 issue needs your attention.",
            _ => $"{summary.IssueCount} issues need your attention."
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
