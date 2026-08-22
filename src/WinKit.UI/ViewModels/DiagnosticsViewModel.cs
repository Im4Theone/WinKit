using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Windows.Data;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;
using WinKit.Diagnostics;
using WinKit.UI.Navigation;

namespace WinKit.UI.ViewModels;

public sealed partial class DiagnosticsViewModel : ObservableObject, IAsyncInitializable
{
    private readonly IDiagnosticsRunner _diagnosticsRunner;
    private readonly IActivityLogService _activityLog;
    private readonly ICollectionView _resultsView;

    public DiagnosticsViewModel(IDiagnosticsRunner diagnosticsRunner, IActivityLogService activityLog)
    {
        _diagnosticsRunner = diagnosticsRunner;
        _activityLog = activityLog;

        _resultsView = CollectionViewSource.GetDefaultView(Results);
        _resultsView.Filter = item => !ShowOnlyIssues || ((DiagnosticCheckResult)item).Status != DiagnosticStatus.Ok;
    }

    public ObservableCollection<DiagnosticCheckResult> Results { get; } = new();

    [ObservableProperty]
    private bool _isRunning;

    [ObservableProperty]
    private bool _showOnlyIssues;

    partial void OnShowOnlyIssuesChanged(bool value) => _resultsView.Refresh();

    public int IssueCount => Results.Count(r => r.Status != DiagnosticStatus.Ok);

    public bool IsHealthy => !IsRunning && IssueCount == 0;

    public Task InitializeAsync() => RunAsync();

    [RelayCommand]
    private async Task RunAsync()
    {
        IsRunning = true;
        Results.Clear();
        OnPropertyChanged(nameof(IssueCount));
        OnPropertyChanged(nameof(IsHealthy));

        try
        {
            var progress = new Progress<DiagnosticCheckResult>(r =>
            {
                Results.Add(r);
                OnPropertyChanged(nameof(IssueCount));
            });

            await _diagnosticsRunner.RunAllAsync(progress);

            if (IssueCount == 0)
            {
                _activityLog.Record("Diagnostics completed - no issues found", ActivityKind.Success);
            }
            else
            {
                _activityLog.Record($"Diagnostics completed - {IssueCount} issue(s) found", ActivityKind.Warning);
            }
        }
        finally
        {
            IsRunning = false;
            OnPropertyChanged(nameof(IsHealthy));
        }
    }

    [RelayCommand]
    private void ReviewIssues() => ShowOnlyIssues = true;
}
