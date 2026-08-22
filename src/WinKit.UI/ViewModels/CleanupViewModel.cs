using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Cleanup.Models;
using WinKit.Cleanup.Services;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;
using WinKit.UI.Dialogs;
using WinKit.UI.Navigation;

namespace WinKit.UI.ViewModels;

public sealed partial class CleanupViewModel : ObservableObject, IAsyncInitializable
{
    private readonly ICleanupScanner _scanner;
    private readonly ICleanupExecutor _executor;
    private readonly IDialogService _dialogService;
    private readonly INotificationService _notificationService;
    private readonly IActivityLogService _activityLog;

    public CleanupViewModel(
        ICleanupScanner scanner,
        ICleanupExecutor executor,
        IDialogService dialogService,
        INotificationService notificationService,
        IActivityLogService activityLog)
    {
        _scanner = scanner;
        _executor = executor;
        _dialogService = dialogService;
        _notificationService = notificationService;
        _activityLog = activityLog;
    }

    public ObservableCollection<CleanupCategoryItemViewModel> Categories { get; } = new();

    [ObservableProperty]
    private bool _isScanning;

    [ObservableProperty]
    private bool _isCleaning;

    public double SelectedTotalGb => Categories.Where(c => c.IsSelected).Sum(c => c.Scan.SizeGb);

    public Task InitializeAsync() => ScanAsync();

    [RelayCommand]
    private async Task ScanAsync()
    {
        IsScanning = true;
        try
        {
            var results = await _scanner.ScanAsync();
            var previousSelection = Categories.ToDictionary(c => c.Scan.Category, c => c.IsSelected);

            Categories.Clear();
            foreach (var scan in results)
            {
                var defaultSelected = scan.Category != CleanupCategory.RecycleBin;
                var isSelected = previousSelection.GetValueOrDefault(scan.Category, defaultSelected);
                var item = new CleanupCategoryItemViewModel(scan, isSelected);
                item.PropertyChanged += (_, _) => OnPropertyChanged(nameof(SelectedTotalGb));
                Categories.Add(item);
            }

            OnPropertyChanged(nameof(SelectedTotalGb));
        }
        finally
        {
            IsScanning = false;
        }
    }

    [RelayCommand]
    private async Task CleanSelectedAsync()
    {
        var selected = Categories.Where(c => c.IsSelected).ToList();
        if (selected.Count == 0)
        {
            return;
        }

        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = "Clean selected items",
            Message = $"This will remove {selected.Sum(c => c.Scan.SizeGb):0.##} GB across {selected.Count} categor{(selected.Count == 1 ? "y" : "ies")}. This can't be undone.",
            ConfirmText = "Clean up",
            IsDestructive = true
        });

        if (!confirmed)
        {
            return;
        }

        IsCleaning = true;
        try
        {
            var categories = selected.Select(c => c.Scan.Category).ToList();
            var result = await _executor.CleanAsync(categories);

            if (result.Success)
            {
                var summary = result.Value!;
                _activityLog.Record($"{summary.GbFreed:0.##} GB of files removed", ActivityKind.Success);
                _notificationService.Show(new NotificationRequest
                {
                    Title = "Cleanup completed",
                    Description = $"{summary.GbFreed:0.##} GB of temporary files removed.",
                    Severity = NotificationSeverity.Success
                });

                if (!string.IsNullOrEmpty(result.UserMessage))
                {
                    _notificationService.Show(new NotificationRequest
                    {
                        Title = "Some items were skipped",
                        Description = result.UserMessage,
                        Severity = NotificationSeverity.Warning
                    });
                }
            }
            else
            {
                _dialogService.ShowError("Cleanup failed", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
            }

            await ScanAsync();
        }
        finally
        {
            IsCleaning = false;
        }
    }
}
