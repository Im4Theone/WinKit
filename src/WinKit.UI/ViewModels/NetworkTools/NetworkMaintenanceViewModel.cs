using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;
using WinKit.Network.Services;
using WinKit.UI.Dialogs;

namespace WinKit.UI.ViewModels.NetworkTools;

public sealed partial class NetworkMaintenanceViewModel : ObservableObject
{
    private readonly INetworkMaintenanceService _maintenanceService;
    private readonly IDialogService _dialogService;
    private readonly INotificationService _notificationService;
    private readonly IActivityLogService _activityLog;

    public NetworkMaintenanceViewModel(
        INetworkMaintenanceService maintenanceService,
        IDialogService dialogService,
        INotificationService notificationService,
        IActivityLogService activityLog)
    {
        _maintenanceService = maintenanceService;
        _dialogService = dialogService;
        _notificationService = notificationService;
        _activityLog = activityLog;
    }

    [ObservableProperty]
    private bool _isBusy;

    [RelayCommand]
    private Task FlushDnsAsync() => RunAsync(
        "Flush DNS", "This clears your computer's cached DNS lookups.", "Flush DNS", isDestructive: false,
        _maintenanceService.FlushDnsAsync, "DNS cache flushed");

    [RelayCommand]
    private Task DhcpRenewAsync() => RunAsync(
        "Renew DHCP lease", "This requests a new IP address from your network's DHCP server.", "Renew", isDestructive: false,
        _maintenanceService.DhcpRenewAsync, "DHCP lease renewed");

    [RelayCommand]
    private Task DhcpReleaseAsync() => RunAsync(
        "Release DHCP lease", "This releases your current IP address. You'll briefly lose network connectivity.", "Release", isDestructive: true,
        _maintenanceService.DhcpReleaseAsync, "DHCP lease released");

    [RelayCommand]
    private Task ResetWinsockAsync() => RunAsync(
        "Reset Winsock", "This resets the Windows network stack. Your computer may need to be restarted.", "Reset Winsock", isDestructive: true,
        _maintenanceService.ResetWinsockAsync, "Winsock reset");

    [RelayCommand]
    private Task ResetTcpIpAsync() => RunAsync(
        "Reset TCP/IP", "This resets the TCP/IP stack to its default configuration. Your computer may need to be restarted.", "Reset TCP/IP", isDestructive: true,
        _maintenanceService.ResetTcpIpAsync, "TCP/IP stack reset");

    private async Task RunAsync(
        string title, string message, string confirmText, bool isDestructive,
        Func<CancellationToken, Task<OperationResult>> operation, string successTitle)
    {
        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = title,
            Message = message,
            ConfirmText = confirmText,
            IsDestructive = isDestructive
        });

        if (!confirmed || IsBusy)
        {
            return;
        }

        IsBusy = true;
        try
        {
            var result = await operation(CancellationToken.None);
            if (result.Success)
            {
                _activityLog.Record(successTitle, ActivityKind.Success);
                _notificationService.Show(new NotificationRequest { Title = successTitle, Severity = NotificationSeverity.Success });
            }
            else
            {
                _dialogService.ShowError($"{title} failed", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
            }
        }
        finally
        {
            IsBusy = false;
        }
    }
}
