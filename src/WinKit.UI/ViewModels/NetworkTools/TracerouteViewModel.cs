using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Network.Models;
using WinKit.Network.Services;

namespace WinKit.UI.ViewModels.NetworkTools;

public sealed partial class TracerouteViewModel : ObservableObject
{
    private readonly ITracerouteService _tracerouteService;
    private CancellationTokenSource? _cts;

    public TracerouteViewModel(ITracerouteService tracerouteService)
    {
        _tracerouteService = tracerouteService;
    }

    [ObservableProperty]
    private string _host = "8.8.8.8";

    [ObservableProperty]
    private bool _isRunning;

    public ObservableCollection<TracerouteHop> Hops { get; } = new();

    [RelayCommand]
    private async Task RunAsync()
    {
        if (string.IsNullOrWhiteSpace(Host) || IsRunning)
        {
            return;
        }

        Hops.Clear();
        IsRunning = true;
        _cts = new CancellationTokenSource();

        try
        {
            var progress = new Progress<TracerouteHop>(h => Hops.Add(h));
            await _tracerouteService.TraceAsync(Host.Trim(), progress, _cts.Token);
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
