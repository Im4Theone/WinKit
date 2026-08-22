namespace WinKit.Core.Abstractions;

/// <summary>
/// Persists simple application settings (behavior toggles, personalization)
/// to local disk. No account, no network, no telemetry.
/// </summary>
public interface IAppSettingsService
{
    bool StartMinimized { get; set; }
    bool StartWithWindows { get; set; }
    bool NotificationsEnabled { get; set; }
    string ThemeName { get; set; }
    string? AccentOverrideHex { get; set; }

    event EventHandler? SettingsChanged;

    Task LoadAsync();
    Task SaveAsync();
}
