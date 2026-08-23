using System.Reflection;
using System.Windows;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;
using WinKit.UI.Dialogs;

namespace WinKit.UI.ViewModels;

public sealed partial class AboutViewModel : ObservableObject
{
    private readonly IUpdateService _updateService;
    private readonly IAppSettingsService _settingsService;
    private readonly IDialogService _dialogService;
    private readonly IActivityLogService _activityLog;

    private UpdateCheckResult? _lastCheckResult;

    public AboutViewModel(
        IUpdateService updateService,
        IAppSettingsService settingsService,
        IDialogService dialogService,
        IActivityLogService activityLog)
    {
        _updateService = updateService;
        _settingsService = settingsService;
        _dialogService = dialogService;
        _activityLog = activityLog;
    }

    public string Version { get; } =
        Assembly.GetExecutingAssembly().GetName().Version?.ToString(3) ?? "1.0.0";

    [ObservableProperty]
    private string _updateStatusText = "Check for updates to see if a newer version is available.";

    [ObservableProperty]
    private bool _isCheckingForUpdates;

    [ObservableProperty]
    private bool _updateAvailable;

    /// <summary>
    /// Called when the user acts on an update-available notification: seeds the page
    /// with the already-known result (no need to re-check) and immediately opens the
    /// same changelog confirmation the manual "Update" button uses.
    /// </summary>
    public async Task ShowPendingUpdateAsync(UpdateCheckResult result)
    {
        _lastCheckResult = result;
        UpdateAvailable = result.Status == UpdateCheckStatus.UpdateAvailable;
        UpdateStatusText = UpdateAvailable
            ? $"WinKit {result.LatestVersion} is available."
            : "You're on the latest version.";

        if (UpdateAvailable)
        {
            await InstallUpdateAsync();
        }
    }

    [RelayCommand]
    private async Task CheckForUpdatesAsync()
    {
        IsCheckingForUpdates = true;
        UpdateStatusText = "Checking for updates...";

        try
        {
            var result = await _updateService.CheckForUpdateAsync();
            _lastCheckResult = result;

            _settingsService.LastUpdateCheckUtc = DateTimeOffset.UtcNow;
            await _settingsService.SaveAsync();

            UpdateAvailable = result.Status == UpdateCheckStatus.UpdateAvailable;
            UpdateStatusText = result.Status switch
            {
                UpdateCheckStatus.UpToDate => "You're on the latest version.",
                UpdateCheckStatus.UpdateAvailable => $"WinKit {result.LatestVersion} is available.",
                _ => $"Couldn't check for updates: {result.ErrorMessage}"
            };
        }
        finally
        {
            IsCheckingForUpdates = false;
        }
    }

    [RelayCommand]
    private async Task InstallUpdateAsync()
    {
        if (_lastCheckResult is not { Status: UpdateCheckStatus.UpdateAvailable })
        {
            return;
        }

        var confirmed = _dialogService.ShowUpdateConfirmation(_lastCheckResult.LatestVersion ?? "", _lastCheckResult.ReleaseNotes);

        if (!confirmed)
        {
            return;
        }

        UpdateStatusText = "Downloading update...";
        var result = await _updateService.DownloadAndApplyUpdateAsync(_lastCheckResult);

        if (result.Status == UpdateApplyStatus.Started)
        {
            _activityLog.Record($"Updating to {_lastCheckResult.LatestVersion}", ActivityKind.Info);
            Application.Current.Shutdown();
        }
        else
        {
            UpdateStatusText = $"Update failed: {result.ErrorMessage}";
            _dialogService.ShowError("Update failed", result.ErrorMessage ?? "The update could not be applied.", null);
        }
    }

    [RelayCommand]
    private async Task SkipThisVersionAsync()
    {
        if (_lastCheckResult?.LatestVersion is null)
        {
            return;
        }

        _settingsService.SkippedUpdateVersion = _lastCheckResult.LatestVersion;
        await _settingsService.SaveAsync();

        UpdateAvailable = false;
        UpdateStatusText = $"Skipped version {_lastCheckResult.LatestVersion}. You'll be notified again for future releases.";
    }
}
