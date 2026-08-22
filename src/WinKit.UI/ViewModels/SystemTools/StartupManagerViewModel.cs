using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.SystemTools.Models;
using WinKit.SystemTools.Services;
using WinKit.UI.Dialogs;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class StartupManagerViewModel : ObservableObject
{
    private readonly IStartupManagerService _startupManagerService;
    private readonly IDialogService _dialogService;

    public StartupManagerViewModel(IStartupManagerService startupManagerService, IDialogService dialogService)
    {
        _startupManagerService = startupManagerService;
        _dialogService = dialogService;
    }

    public ObservableCollection<StartupEntry> Entries { get; } = new();

    [ObservableProperty]
    private bool _isLoading;

    [RelayCommand]
    private async Task RefreshAsync()
    {
        IsLoading = true;
        try
        {
            var entries = await _startupManagerService.GetEntriesAsync();
            Entries.Clear();
            foreach (var entry in entries)
            {
                Entries.Add(entry);
            }
        }
        finally
        {
            IsLoading = false;
        }
    }

    [RelayCommand]
    private async Task ToggleAsync(StartupEntry entry)
    {
        var result = await _startupManagerService.SetEnabledAsync(entry, !entry.IsEnabled);
        if (!result.Success)
        {
            _dialogService.ShowError("Couldn't change startup item", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
        }

        await RefreshAsync();
    }
}
