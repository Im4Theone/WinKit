using System.Collections.ObjectModel;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.SystemTools.Models;
using WinKit.SystemTools.Services;
using WinKit.UI.Dialogs;
using WinKit.UI.Navigation;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class ProcessManagerViewModel : ObservableObject
{
    private readonly IProcessManagerService _processManagerService;
    private readonly IDialogService _dialogService;
    private readonly INavigationService _navigationService;
    private readonly DispatcherTimer _refreshTimer;
    private bool _isRefreshing;

    public ProcessManagerViewModel(
        IProcessManagerService processManagerService, IDialogService dialogService, INavigationService navigationService)
    {
        _processManagerService = processManagerService;
        _dialogService = dialogService;
        _navigationService = navigationService;

        _refreshTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        _refreshTimer.Tick += async (_, _) => await RefreshAsync();

        // Enumerating every running process every second is real, ongoing work - only do it
        // while System Tools is actually the visible page, not for the rest of the app session.
        _navigationService.Navigated += (_, _) => UpdateTimerState();
        UpdateTimerState();
    }

    private void UpdateTimerState()
    {
        if (IsFrozen)
        {
            return;
        }

        if (_navigationService.CurrentViewModel is SystemToolsViewModel)
        {
            _refreshTimer.Start();
        }
        else
        {
            _refreshTimer.Stop();
        }
    }

    public ObservableCollection<ProcessEntry> Processes { get; } = new();

    [ObservableProperty]
    private bool _isLoading;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(FreezeButtonText))]
    private bool _isFrozen;

    public string FreezeButtonText => IsFrozen ? "Unfreeze" : "Freeze";

    [RelayCommand]
    private async Task RefreshAsync()
    {
        if (_isRefreshing)
        {
            return;
        }

        _isRefreshing = true;
        IsLoading = true;
        try
        {
            var processes = await _processManagerService.GetProcessesAsync();
            Processes.Clear();
            foreach (var process in processes)
            {
                Processes.Add(process);
            }
        }
        finally
        {
            IsLoading = false;
            _isRefreshing = false;
        }
    }

    [RelayCommand]
    private async Task ToggleFreezeAsync()
    {
        IsFrozen = !IsFrozen;

        if (IsFrozen)
        {
            _refreshTimer.Stop();
        }
        else
        {
            _refreshTimer.Start();
            await RefreshAsync();
        }
    }

    [RelayCommand]
    private async Task EndProcessAsync(ProcessEntry entry)
    {
        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = "End process",
            Message = $"End \"{entry.Name}\" (PID {entry.Id})? Unsaved work in this process will be lost.",
            ConfirmText = "End process",
            IsDestructive = true
        });

        if (!confirmed)
        {
            return;
        }

        var result = _processManagerService.EndProcess(entry.Id);
        if (!result.Success)
        {
            _dialogService.ShowError("Couldn't end process", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
        }

        if (!IsFrozen)
        {
            await RefreshAsync();
        }
    }
}
