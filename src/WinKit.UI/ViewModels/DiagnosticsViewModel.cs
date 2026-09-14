using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Windows.Data;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;
using WinKit.Diagnostics;
using WinKit.UI.Dialogs;
using WinKit.UI.Navigation;
using WinKit.UI.ViewModels.SystemTools;

namespace WinKit.UI.ViewModels;

public sealed partial class DiagnosticsViewModel : ObservableObject, IAsyncInitializable
{
    private readonly IDiagnosticEngine _engine;
    private readonly IDialogService _dialogService;
    private readonly INotificationService _notificationService;
    private readonly IActivityLogService _activityLog;
    private readonly INavigationService _navigationService;
    private readonly ICollectionView _resultsView;

    private CancellationTokenSource? _scanCts;
    private CancellationTokenSource? _fixCts;

    public DiagnosticsViewModel(
        IDiagnosticEngine engine,
        IDialogService dialogService,
        INotificationService notificationService,
        IActivityLogService activityLog,
        INavigationService navigationService)
    {
        _engine = engine;
        _dialogService = dialogService;
        _notificationService = notificationService;
        _activityLog = activityLog;
        _navigationService = navigationService;

        _resultsView = CollectionViewSource.GetDefaultView(Results);
        _resultsView.Filter = item => !ShowOnlyIssues || ((DiagnosticCheckItemViewModel)item).IsIssue;
        _resultsView.GroupDescriptions.Add(new PropertyGroupDescription(nameof(DiagnosticCheckItemViewModel.CategoryLabel)));
        _resultsView.SortDescriptions.Add(
            new SortDescription(nameof(DiagnosticCheckItemViewModel.SortPriority), ListSortDirection.Ascending));
    }

    public ObservableCollection<DiagnosticCheckItemViewModel> Results { get; } = new();

    /// <summary>Filtered/grouped view over Results; the view binds its list to this, not to Results directly.</summary>
    public ICollectionView ResultsView => _resultsView;

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(CancelScanCommand))]
    [NotifyCanExecuteChangedFor(nameof(ActCommand))]
    [NotifyPropertyChangedFor(nameof(ShowSummary))]
    [NotifyPropertyChangedFor(nameof(IsHealthy))]
    private bool _isRunning;

    /// <summary>True while any single fix/repair is in flight. Only one can run at a time.</summary>
    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(CancelFixCommand))]
    [NotifyCanExecuteChangedFor(nameof(RunCommand))]
    private bool _isFixRunning;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ShowSummary))]
    [NotifyPropertyChangedFor(nameof(IsHealthy))]
    private bool _hasRun;

    [ObservableProperty]
    private bool _showOnlyIssues;

    partial void OnShowOnlyIssuesChanged(bool value) => _resultsView.Refresh();

    /// <summary>Whether the results summary card should be shown: only once a run has finished.</summary>
    public bool ShowSummary => HasRun && !IsRunning;

    public int PassedCount => Results.Count(r => r.Result.Status == DiagnosticStatus.Passed);
    public int WarningCount => Results.Count(r => r.Result.Status == DiagnosticStatus.Warning);
    public int FailedCount => Results.Count(r => r.Result.Status == DiagnosticStatus.Failed);
    public int IssueCount => WarningCount + FailedCount;
    public bool IsHealthy => HasRun && !IsRunning && IssueCount == 0;

    public Task InitializeAsync() => RunAsync();

    [RelayCommand(AllowConcurrentExecutions = false, CanExecute = nameof(CanRun))]
    private async Task RunAsync()
    {
        _scanCts?.Dispose();
        _scanCts = new CancellationTokenSource();

        IsRunning = true;
        Results.Clear();
        NotifyCounts();

        try
        {
            var progress = new Progress<DiagnosticCheckResult>(r =>
            {
                Results.Add(new DiagnosticCheckItemViewModel(r));
                NotifyCounts();
            });

            await _engine.RunAllAsync(progress, _scanCts.Token);

            HasRun = true;

            if (IssueCount == 0)
            {
                _activityLog.Record("Diagnostics completed - no issues found", ActivityKind.Success);
            }
            else
            {
                _activityLog.Record($"Diagnostics completed - {IssueCount} issue(s) found", ActivityKind.Warning);
            }
        }
        catch (OperationCanceledException)
        {
            _activityLog.Record("Diagnostics scan cancelled", ActivityKind.Info);
        }
        finally
        {
            IsRunning = false;
            NotifyCounts();
        }
    }

    private bool CanRun() => !IsFixRunning;

    [RelayCommand(CanExecute = nameof(IsRunning))]
    private void CancelScan() => _scanCts?.Cancel();

    [RelayCommand]
    private void ReviewIssues() => ShowOnlyIssues = true;

    [RelayCommand(AllowConcurrentExecutions = false, CanExecute = nameof(CanAct))]
    private async Task ActAsync(DiagnosticCheckItemViewModel item)
    {
        var action = item.Result.FixAction;
        if (action is null)
        {
            return;
        }

        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = $"{action.Label} - {item.Result.Name}",
            Message = action.ConfirmationMessage +
                      (action.RequiresElevation ? " Windows may prompt for administrator permission." : string.Empty),
            ConfirmText = action.Label,
            IsDestructive = action.IsDestructive
        });

        if (!confirmed)
        {
            return;
        }

        if (action.Kind == DiagnosticActionKind.Review)
        {
            NavigateForReview(item.Result.CheckId);
            return;
        }

        _fixCts?.Dispose();
        _fixCts = new CancellationTokenSource();
        IsFixRunning = true;
        item.IsBusy = true;
        try
        {
            var progress = new Progress<string>(message => item.StatusMessage = message);
            var result = await _engine.FixAsync(item.Result.CheckId, progress, _fixCts.Token);

            if (result.Success)
            {
                var summary = result.Value ?? "Completed.";
                _activityLog.Record($"{item.Result.Name}: {summary}", ActivityKind.Success);
                _notificationService.Show(new NotificationRequest
                {
                    Title = $"{item.Result.Name} fixed",
                    Description = summary,
                    Severity = NotificationSeverity.Success
                });

                if (!string.IsNullOrEmpty(result.UserMessage))
                {
                    _notificationService.Show(new NotificationRequest
                    {
                        Title = "Note",
                        Description = result.UserMessage,
                        Severity = NotificationSeverity.Warning
                    });
                }
            }
            else
            {
                _activityLog.Record($"{item.Result.Name}: fix failed", ActivityKind.Error);
                _dialogService.ShowError(
                    $"{action.Label} failed", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
            }
        }
        catch (OperationCanceledException)
        {
            _activityLog.Record($"{item.Result.Name}: fix cancelled", ActivityKind.Info);
        }
        finally
        {
            item.IsBusy = false;
            item.StatusMessage = null;
            IsFixRunning = false;
        }

        await RunAsync();
    }

    private bool CanAct() => !IsRunning;

    [RelayCommand(CanExecute = nameof(IsFixRunning))]
    private void CancelFix() => _fixCts?.Cancel();

    private void NavigateForReview(string checkId)
    {
        switch (checkId)
        {
            case DiagnosticCheckIds.Startup:
                _navigationService.NavigateTo<SystemToolsViewModel>().SelectTab<StartupManagerViewModel>();
                break;
            case DiagnosticCheckIds.Services:
                _navigationService.NavigateTo<SystemToolsViewModel>().SelectTab<ServicesManagerViewModel>();
                break;
            case DiagnosticCheckIds.DiskSpace:
                _navigationService.NavigateTo<CleanupViewModel>();
                break;
        }
    }

    private void NotifyCounts()
    {
        OnPropertyChanged(nameof(PassedCount));
        OnPropertyChanged(nameof(WarningCount));
        OnPropertyChanged(nameof(FailedCount));
        OnPropertyChanged(nameof(IssueCount));
        OnPropertyChanged(nameof(IsHealthy));
    }
}
