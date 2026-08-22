using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.SystemTools.Models;
using WinKit.SystemTools.Services;
using WinKit.UI.Dialogs;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class InstalledAppsViewModel : ObservableObject
{
    private readonly IInstalledAppsService _installedAppsService;
    private readonly IDialogService _dialogService;

    public InstalledAppsViewModel(IInstalledAppsService installedAppsService, IDialogService dialogService)
    {
        _installedAppsService = installedAppsService;
        _dialogService = dialogService;
    }

    public ObservableCollection<InstalledApp> Apps { get; } = new();

    [ObservableProperty]
    private bool _isLoading;

    [RelayCommand]
    private async Task RefreshAsync()
    {
        IsLoading = true;
        try
        {
            var apps = await _installedAppsService.GetInstalledAppsAsync();
            Apps.Clear();
            foreach (var app in apps)
            {
                Apps.Add(app);
            }
        }
        finally
        {
            IsLoading = false;
        }
    }

    [RelayCommand]
    private void Uninstall(InstalledApp app)
    {
        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = "Uninstall application",
            Message = $"This opens the uninstaller for \"{app.Name}\". WinKit doesn't remove anything itself — you'll finish the uninstall in the app's own wizard.",
            ConfirmText = "Continue",
            IsDestructive = true
        });

        if (!confirmed)
        {
            return;
        }

        var result = _installedAppsService.LaunchUninstaller(app);
        if (!result.Success)
        {
            _dialogService.ShowError("Couldn't start uninstaller", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
        }
    }
}
