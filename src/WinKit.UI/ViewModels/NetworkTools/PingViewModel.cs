using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Network.Models;
using WinKit.Network.Services;

namespace WinKit.UI.ViewModels.NetworkTools;

public sealed partial class PingViewModel : ObservableObject
{
    private readonly IPingService _pingService;
    private CancellationTokenSource? _cts;

    public PingViewModel(IPingService pingService)
    {
        _pingService = pingService;
    }

    [ObservableProperty]
    private string _host = "8.8.8.8";

    [ObservableProperty]
    private bool _isRunning;

    public ObservableCollection<PingReply> Results { get; } = new();

    [RelayCommand]
    private async Task RunAsync()
    {
        if (string.IsNullOrWhiteSpace(Host) || IsRunning)
        {
            return;
        }

        Results.Clear();
        IsRunning = true;
        _cts = new CancellationTokenSource();

        try
        {
            var progress = new Progress<PingReply>(r => Results.Add(r));
            await _pingService.PingAsync(Host.Trim(), 4, progress, _cts.Token);
        }
        catch (OperationCanceledException)
        {
            // Stopped by the user.
        }
        finally
        {
            IsRunning = false;
        }
    }

    [RelayCommand]
    private void Stop() => _cts?.Cancel();
}
