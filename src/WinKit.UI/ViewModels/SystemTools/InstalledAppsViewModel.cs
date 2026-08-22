using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.SystemTools.Services;
using WinKit.UI.Dialogs;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class InstalledAppsViewModel : ObservableObject
{
    private static readonly TimeSpan LaunchFeedbackDuration = TimeSpan.FromSeconds(2);

    private readonly IInstalledAppsService _installedAppsService;
    private readonly IDialogService _dialogService;

    public InstalledAppsViewModel(IInstalledAppsService installedAppsService, IDialogService dialogService)
    {
        _installedAppsService = installedAppsService;
        _dialogService = dialogService;
    }

    public ObservableCollection<InstalledAppItemViewModel> Apps { get; } = new();

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
                Apps.Add(new InstalledAppItemViewModel(app));
            }
        }
        finally
        {
            IsLoading = false;
        }
    }

    [RelayCommand]
    private async Task UninstallAsync(InstalledAppItemViewModel item)
    {
        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = "Uninstall application",
            Message = $"This opens the uninstaller for \"{item.App.Name}\". WinKit doesn't remove anything itself — you'll finish the uninstall in the app's own wizard.",
            ConfirmText = "Continue",
            IsDestructive = true
        });

        if (!confirmed)
        {
            return;
        }

        item.IsUninstalling = true;
        try
        {
            var result = _installedAppsService.LaunchUninstaller(item.App);
            if (!result.Success)
            {
                _dialogService.ShowError("Couldn't start uninstaller", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
                return;
            }

            // The uninstaller is a separate process WinKit doesn't track to completion;
            // this just holds the busy indicator long enough to confirm the launch went through.
            await Task.Delay(LaunchFeedbackDuration);
        }
        finally
        {
            item.IsUninstalling = false;
        }
    }
}
