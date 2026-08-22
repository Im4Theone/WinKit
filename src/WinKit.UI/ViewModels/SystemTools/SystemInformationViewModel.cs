using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.SystemTools.Models;
using WinKit.SystemTools.Services;
using WinKit.UI.Formatting;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class SystemInformationViewModel : ObservableObject
{
    private readonly ISystemInfoService _systemInfoService;

    public SystemInformationViewModel(ISystemInfoService systemInfoService)
    {
        _systemInfoService = systemInfoService;

        var uptimeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        uptimeTimer.Tick += (_, _) => UptimeText = UptimeFormatter.Format(TimeSpan.FromMilliseconds(Environment.TickCount64));
        uptimeTimer.Start();
    }

    public string MachineName { get; } = Environment.MachineName;
    public string Architecture { get; } = Environment.Is64BitOperatingSystem ? "64-bit" : "32-bit";

    [ObservableProperty]
    private SystemOverview? _overview;

    [ObservableProperty]
    private bool _isLoading;

    [ObservableProperty]
    private string _uptimeText = UptimeFormatter.Format(TimeSpan.FromMilliseconds(Environment.TickCount64));

    [RelayCommand]
    private async Task RefreshAsync()
    {
        IsLoading = true;
        try
        {
            Overview = await _systemInfoService.GetOverviewAsync();
        }
        finally
        {
            IsLoading = false;
        }
    }
}
