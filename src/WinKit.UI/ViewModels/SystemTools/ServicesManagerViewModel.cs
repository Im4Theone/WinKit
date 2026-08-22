using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.SystemTools.Models;
using WinKit.SystemTools.Services;
using WinKit.UI.Dialogs;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class ServicesManagerViewModel : ObservableObject
{
    private readonly IServicesManagerService _servicesManagerService;
    private readonly IDialogService _dialogService;

    public ServicesManagerViewModel(IServicesManagerService servicesManagerService, IDialogService dialogService)
    {
        _servicesManagerService = servicesManagerService;
        _dialogService = dialogService;
    }

    public ObservableCollection<ServiceEntry> Services { get; } = new();

    [ObservableProperty]
    private bool _isLoading;

    [RelayCommand]
    private async Task RefreshAsync()
    {
        IsLoading = true;
        try
        {
            var services = await _servicesManagerService.GetServicesAsync();
            Services.Clear();
            foreach (var service in services)
            {
                Services.Add(service);
            }
        }
        finally
        {
            IsLoading = false;
        }
    }

    [RelayCommand]
    private async Task StartAsync(ServiceEntry entry)
    {
        var result = await _servicesManagerService.StartAsync(entry.Name);
        if (!result.Success)
        {
            _dialogService.ShowError("Couldn't start service", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
        }

        await RefreshAsync();
    }

    [RelayCommand]
    private async Task StopAsync(ServiceEntry entry)
    {
        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = "Stop service",
            Message = $"Stop \"{entry.DisplayName}\"? Some features of Windows or other apps may stop working until it's started again.",
            ConfirmText = "Stop service",
            IsDestructive = true
        });

        if (!confirmed)
        {
            return;
        }

        var result = await _servicesManagerService.StopAsync(entry.Name);
        if (!result.Success)
        {
            _dialogService.ShowError("Couldn't stop service", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
        }

        await RefreshAsync();
    }
}
