using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.SystemTools.Models;
using WinKit.SystemTools.Services;
using WinKit.UI.Dialogs;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class ProcessManagerViewModel : ObservableObject
{
    private readonly IProcessManagerService _processManagerService;
    private readonly IDialogService _dialogService;

    public ProcessManagerViewModel(IProcessManagerService processManagerService, IDialogService dialogService)
    {
        _processManagerService = processManagerService;
        _dialogService = dialogService;
    }

    public ObservableCollection<ProcessEntry> Processes { get; } = new();

    [ObservableProperty]
    private bool _isLoading;

    [RelayCommand]
    private async Task RefreshAsync()
    {
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

        await RefreshAsync();
    }
}
