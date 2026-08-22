using WinKit.Core.Abstractions;

namespace WinKit.Infrastructure;

public sealed class AppSettingsService : IAppSettingsService
{
    private sealed class SettingsData
    {
        public bool StartMinimized { get; set; }
        public bool StartWithWindows { get; set; }
        public bool NotificationsEnabled { get; set; } = true;
        public string ThemeName { get; set; } = "Midnight";
        public string? AccentOverrideHex { get; set; }
    }

    private SettingsData _data = new();

    public event EventHandler? SettingsChanged;

    public bool StartMinimized
    {
        get => _data.StartMinimized;
        set { _data.StartMinimized = value; RaiseChanged(); }
    }

    public bool StartWithWindows
    {
        get => _data.StartWithWindows;
        set { _data.StartWithWindows = value; RaiseChanged(); }
    }

    public bool NotificationsEnabled
    {
        get => _data.NotificationsEnabled;
        set { _data.NotificationsEnabled = value; RaiseChanged(); }
    }

    public string ThemeName
    {
        get => _data.ThemeName;
        set { _data.ThemeName = value; RaiseChanged(); }
    }

    public string? AccentOverrideHex
    {
        get => _data.AccentOverrideHex;
        set { _data.AccentOverrideHex = value; RaiseChanged(); }
    }

    public async Task LoadAsync()
    {
        _data = await JsonFileStore.ReadAsync<SettingsData>(AppPaths.SettingsFile) ?? new SettingsData();
    }

    public Task SaveAsync() => JsonFileStore.WriteAsync(AppPaths.SettingsFile, _data);

    private void RaiseChanged() => SettingsChanged?.Invoke(this, EventArgs.Empty);
}
