namespace WinKit.Infrastructure;

/// <summary>
/// Central definition of where WinKit stores local, non-roaming data.
/// Everything lives under %LOCALAPPDATA%\WinKit — no cloud sync, no telemetry.
/// </summary>
public static class AppPaths
{
    public static string RootDirectory { get; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "WinKit");

    public static string ThemesDirectory { get; } = Path.Combine(RootDirectory, "Themes");

    public static string LogsDirectory { get; } = Path.Combine(RootDirectory, "Logs");

    public static string ProfileFile { get; } = Path.Combine(RootDirectory, "profile.json");

    public static string SettingsFile { get; } = Path.Combine(RootDirectory, "settings.json");

    public static string ActivityLogFile { get; } = Path.Combine(RootDirectory, "activity.json");

    public static void EnsureCreated()
    {
        Directory.CreateDirectory(RootDirectory);
        Directory.CreateDirectory(ThemesDirectory);
        Directory.CreateDirectory(LogsDirectory);
    }
}
