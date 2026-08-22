using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;
using WinKit.SystemTools.Services;
using WinKit.UI.Dialogs;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class HostsFileViewModel : ObservableObject
{
    private readonly IHostsFileService _hostsFileService;
    private readonly IDialogService _dialogService;
    private readonly INotificationService _notificationService;

    public HostsFileViewModel(
        IHostsFileService hostsFileService, IDialogService dialogService, INotificationService notificationService)
    {
        _hostsFileService = hostsFileService;
        _dialogService = dialogService;
        _notificationService = notificationService;
    }

    public string FilePath => _hostsFileService.HostsFilePath;

    [ObservableProperty]
    private string _content = string.Empty;

    [ObservableProperty]
    private bool _isLoading;

    [RelayCommand]
    private async Task LoadAsync()
    {
        IsLoading = true;
        try
        {
            Content = await _hostsFileService.ReadRawAsync();
        }
        finally
        {
            IsLoading = false;
        }
    }

    [RelayCommand]
    private async Task SaveAsync()
    {
        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = "Save hosts file",
            Message = "This changes how your computer resolves domain names to IP addresses system-wide. " +
                      "Only continue if you're sure about these changes.",
            ConfirmText = "Save",
            IsDestructive = true
        });

        if (!confirmed)
        {
            return;
        }

        var result = await _hostsFileService.WriteRawAsync(Content);
        if (!result.Success)
        {
            _dialogService.ShowError("Couldn't save hosts file", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
            return;
        }

        _notificationService.Show(new NotificationRequest { Title = "Hosts file saved", Severity = NotificationSeverity.Success });
    }
}
