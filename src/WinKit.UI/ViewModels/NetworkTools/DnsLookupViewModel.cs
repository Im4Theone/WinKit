using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Network.Models;
using WinKit.Network.Services;

namespace WinKit.UI.ViewModels.NetworkTools;

public sealed partial class DnsLookupViewModel : ObservableObject
{
    private readonly IDnsLookupService _dnsLookupService;

    public DnsLookupViewModel(IDnsLookupService dnsLookupService)
    {
        _dnsLookupService = dnsLookupService;
    }

    [ObservableProperty]
    private string _host = "example.com";

    [ObservableProperty]
    private bool _isRunning;

    [ObservableProperty]
    private DnsLookupResult? _result;

    [ObservableProperty]
    private string? _errorMessage;

    [RelayCommand]
    private async Task LookupAsync()
    {
        if (string.IsNullOrWhiteSpace(Host) || IsRunning)
        {
            return;
        }

        IsRunning = true;
        Result = null;
        ErrorMessage = null;

        try
        {
            var outcome = await _dnsLookupService.LookupAsync(Host.Trim());
            if (outcome.Success)
            {
                Result = outcome.Value;
            }
            else
            {
                ErrorMessage = outcome.UserMessage;
            }
        }
        finally
        {
            IsRunning = false;
        }
    }
}
