using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Win32;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;
using WinKit.Themes;
using WinKit.UI.Dialogs;
using WinKit.UI.Navigation;

namespace WinKit.UI.ViewModels;

public sealed partial class SettingsViewModel : ObservableObject, IAsyncInitializable
{
    private readonly IThemeService _themeService;
    private readonly IAppSettingsService _settingsService;
    private readonly IUserProfileService _profileService;
    private readonly IElevationService _elevationService;
    private readonly IAutoStartService _autoStartService;
    private readonly IDialogService _dialogService;
    private readonly INotificationService _notificationService;
    private readonly IActivityLogService _activityLog;

    private bool _suppressThemeSelectionApply;
    private string? _editingOriginalName;

    public SettingsViewModel(
        IThemeService themeService,
        IAppSettingsService settingsService,
        IUserProfileService profileService,
        IElevationService elevationService,
        IAutoStartService autoStartService,
        IDialogService dialogService,
        INotificationService notificationService,
        IActivityLogService activityLog)
    {
        _themeService = themeService;
        _settingsService = settingsService;
        _profileService = profileService;
        _elevationService = elevationService;
        _autoStartService = autoStartService;
        _dialogService = dialogService;
        _notificationService = notificationService;
        _activityLog = activityLog;

        _displayName = _profileService.Current.Name;
    }

    public bool IsElevated => _elevationService.IsElevated;

    public System.Collections.ObjectModel.ObservableCollection<string> ThemeNames { get; } = new();

    [ObservableProperty]
    private string _selectedThemeName = ThemeService.SystemThemeName;

    partial void OnSelectedThemeNameChanged(string value)
    {
        if (_suppressThemeSelectionApply)
        {
            return;
        }

        _ = ApplyThemeSelectionAsync(value);
    }

    [ObservableProperty]
    private ThemeDraftViewModel? _editor;

    [ObservableProperty]
    private string _displayName;

    [ObservableProperty]
    private bool _startMinimized;

    partial void OnStartMinimizedChanged(bool value)
    {
        _settingsService.StartMinimized = value;
        _ = _settingsService.SaveAsync();
    }

    [ObservableProperty]
    private bool _startWithWindows;

    partial void OnStartWithWindowsChanged(bool value)
    {
        _autoStartService.SetEnabled(value);
        _settingsService.StartWithWindows = value;
        _ = _settingsService.SaveAsync();
    }

    [ObservableProperty]
    private bool _notificationsEnabled;

    partial void OnNotificationsEnabledChanged(bool value)
    {
        _settingsService.NotificationsEnabled = value;
        _ = _settingsService.SaveAsync();
    }

    public async Task InitializeAsync()
    {
        RefreshThemeList();

        _suppressThemeSelectionApply = true;
        SelectedThemeName = _settingsService.ThemeName;
        _suppressThemeSelectionApply = false;

        StartMinimized = _settingsService.StartMinimized;
        StartWithWindows = _autoStartService.IsEnabled();
        NotificationsEnabled = _settingsService.NotificationsEnabled;
        await Task.CompletedTask;
    }

    private void RefreshThemeList()
    {
        ThemeNames.Clear();
        foreach (var name in new[] { ThemeService.SystemThemeName }
                     .Concat(_themeService.BuiltInThemes.Select(t => t.Name))
                     .Concat(_themeService.CustomThemes.Select(t => t.Name)))
        {
            ThemeNames.Add(name);
        }
    }

    private async Task ApplyThemeSelectionAsync(string name)
    {
        await _themeService.ApplyThemeAsync(name);
        _settingsService.ThemeName = name;
        await _settingsService.SaveAsync();
        _activityLog.Record($"Theme changed to {name}", ActivityKind.Info);
    }

    [RelayCommand]
    private async Task SaveNameAsync()
    {
        if (string.IsNullOrWhiteSpace(DisplayName))
        {
            return;
        }

        await _profileService.SetNameAsync(DisplayName);
        _notificationService.Show(new NotificationRequest { Title = "Name updated", Severity = NotificationSeverity.Success });
    }

    [RelayCommand]
    private void CreateTheme()
    {
        var baseTheme = FindThemeDefinition(SelectedThemeName) ?? _themeService.BuiltInThemes.First();
        _editingOriginalName = null;
        OpenEditor(_themeService.CreateDraftFrom(baseTheme, GenerateNewThemeName(baseTheme.Name)));
    }

    [RelayCommand]
    private void EditCurrentTheme()
    {
        var theme = FindThemeDefinition(SelectedThemeName);
        if (theme is null)
        {
            return;
        }

        _editingOriginalName = theme.IsBuiltIn ? null : theme.Name;
        OpenEditor(theme.IsBuiltIn ? _themeService.CreateDraftFrom(theme, GenerateNewThemeName(theme.Name)) : theme.Clone());
    }

    private void OpenEditor(ThemeDefinition draft)
    {
        Editor = new ThemeDraftViewModel(draft, () => _themeService.Preview(Editor!.ToDefinition()));
        _themeService.Preview(draft);
    }

    [RelayCommand]
    private async Task CloseEditorAsync()
    {
        Editor = null;
        await _themeService.ApplyThemeAsync(SelectedThemeName);
    }

    [RelayCommand]
    private async Task SaveThemeAsync()
    {
        if (Editor is null)
        {
            return;
        }

        if (!_themeService.Validate(Editor.ToDefinition(), out var error))
        {
            _dialogService.ShowError("Invalid theme", error ?? "This theme could not be saved.", null);
            return;
        }

        var definition = Editor.ToDefinition();
        await _themeService.SaveCustomThemeAsync(definition);

        if (_editingOriginalName is not null && _editingOriginalName != definition.Name)
        {
            await _themeService.DeleteCustomThemeAsync(_editingOriginalName);
        }

        RefreshThemeList();
        _suppressThemeSelectionApply = true;
        SelectedThemeName = definition.Name;
        _suppressThemeSelectionApply = false;

        _settingsService.ThemeName = definition.Name;
        await _settingsService.SaveAsync();

        Editor = null;
        _activityLog.Record($"Theme \"{definition.Name}\" saved", ActivityKind.Success);
        _notificationService.Show(new NotificationRequest { Title = "Theme saved", Description = definition.Name, Severity = NotificationSeverity.Success });
    }

    [RelayCommand]
    private async Task ImportThemeAsync()
    {
        var dialog = new OpenFileDialog { Filter = "WinKit theme (*.json)|*.json" };
        if (dialog.ShowDialog() != true)
        {
            return;
        }

        var imported = await _themeService.ImportThemeAsync(dialog.FileName);
        if (imported is null)
        {
            _dialogService.ShowError("Import failed", "That file isn't a valid WinKit theme.", null);
            return;
        }

        RefreshThemeList();
        SelectedThemeName = imported.Name;
        _notificationService.Show(new NotificationRequest { Title = "Theme imported", Description = imported.Name, Severity = NotificationSeverity.Success });
    }

    [RelayCommand]
    private async Task ExportThemeAsync()
    {
        var theme = FindThemeDefinition(SelectedThemeName);
        if (theme is null)
        {
            return;
        }

        var dialog = new SaveFileDialog { Filter = "WinKit theme (*.json)|*.json", FileName = $"{theme.Name}.json" };
        if (dialog.ShowDialog() != true)
        {
            return;
        }

        await _themeService.ExportThemeAsync(theme, dialog.FileName);
        _notificationService.Show(new NotificationRequest { Title = "Theme exported", Description = dialog.FileName, Severity = NotificationSeverity.Success });
    }

    [RelayCommand]
    private async Task ResetThemeAsync()
    {
        await ApplyThemeSelectionAsync("Midnight");
        _suppressThemeSelectionApply = true;
        SelectedThemeName = "Midnight";
        _suppressThemeSelectionApply = false;
    }

    [RelayCommand]
    private async Task DeleteThemeAsync()
    {
        var theme = FindThemeDefinition(SelectedThemeName);
        if (theme is null || theme.IsBuiltIn)
        {
            return;
        }

        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = "Delete theme",
            Message = $"Delete \"{theme.Name}\"? This can't be undone.",
            ConfirmText = "Delete",
            IsDestructive = true
        });

        if (!confirmed)
        {
            return;
        }

        await _themeService.DeleteCustomThemeAsync(theme.Name);
        RefreshThemeList();
        await ResetThemeAsync();
    }

    [RelayCommand]
    private void RunAsAdministrator()
    {
        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = "Restart as Administrator",
            Message = "Are you sure you want to restart the program via Admin?",
            ConfirmText = "Restart as Administrator"
        });

        if (!confirmed)
        {
            return;
        }

        if (_elevationService.RelaunchElevated(string.Empty))
        {
            System.Windows.Application.Current.Shutdown();
        }
        else
        {
            _dialogService.ShowError(
                "Couldn't restart as Administrator",
                "The elevation request was cancelled or failed. WinKit is still running normally.",
                null);
        }
    }

    private ThemeDefinition? FindThemeDefinition(string name) =>
        _themeService.BuiltInThemes.Concat(_themeService.CustomThemes)
            .FirstOrDefault(t => string.Equals(t.Name, name, StringComparison.OrdinalIgnoreCase))
        ?? (name == ThemeService.SystemThemeName ? _themeService.Current : null);

    private string GenerateNewThemeName(string baseName)
    {
        var candidate = $"{baseName} Copy";
        var existing = _themeService.CustomThemes.Select(t => t.Name).ToHashSet(StringComparer.OrdinalIgnoreCase);
        var suffix = 2;
        while (existing.Contains(candidate))
        {
            candidate = $"{baseName} Copy {suffix++}";
        }

        return candidate;
    }
}
