namespace WinKit.Cleanup.Services;

internal static class CleanupPaths
{
    public static string UserTemp => Path.GetTempPath();

    public static string WindowsTemp => Path.Combine(
        Environment.GetEnvironmentVariable("SystemRoot") ?? @"C:\Windows", "Temp");

    public static string ExplorerCache => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "Microsoft", "Windows", "Explorer");

    public static string WindowsUpdateDownloadCache => Path.Combine(
        Environment.GetEnvironmentVariable("SystemRoot") ?? @"C:\Windows",
        "SoftwareDistribution", "Download");

    public static readonly EnumerationOptions SafeRecursiveEnumeration = new()
    {
        RecurseSubdirectories = true,
        IgnoreInaccessible = true,
        AttributesToSkip = FileAttributes.ReparsePoint
    };
}
