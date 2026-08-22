using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Network.Models;
using WinKit.Network.Services;

namespace WinKit.UI.ViewModels.NetworkTools;

public sealed partial class IpConfigurationViewModel : ObservableObject
{
    private readonly IIpConfigurationService _ipConfigurationService;

    public IpConfigurationViewModel(IIpConfigurationService ipConfigurationService)
    {
        _ipConfigurationService = ipConfigurationService;
    }

    public ObservableCollection<AdapterConfiguration> Adapters { get; } = new();

    [ObservableProperty]
    private bool _isLoading;

    [RelayCommand]
    private async Task RefreshAsync()
    {
        IsLoading = true;
        try
        {
            var adapters = await _ipConfigurationService.GetAdaptersAsync();
            Adapters.Clear();
            foreach (var adapter in adapters)
            {
                Adapters.Add(adapter);
            }
        }
        finally
        {
            IsLoading = false;
        }
    }
}
