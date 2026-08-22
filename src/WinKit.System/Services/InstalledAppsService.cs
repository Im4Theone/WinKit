using System.Diagnostics;
using System.Globalization;
using Microsoft.Win32;
using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public sealed class InstalledAppsService : IInstalledAppsService
{
    private static readonly (RegistryKey Hive, string Path)[] UninstallKeys =
    {
        (Registry.LocalMachine, @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"),
        (Registry.LocalMachine, @"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall"),
        (Registry.CurrentUser, @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall")
    };

    public Task<IReadOnlyList<InstalledApp>> GetInstalledAppsAsync(CancellationToken cancellationToken = default)
    {
        return Task.Run<IReadOnlyList<InstalledApp>>(() =>
        {
            var apps = new List<InstalledApp>();

            foreach (var (hive, path) in UninstallKeys)
            {
                using var root = hive.OpenSubKey(path);
                if (root is null)
                {
                    continue;
                }

                foreach (var subKeyName in root.GetSubKeyNames())
                {
                    using var subKey = root.OpenSubKey(subKeyName);
                    var name = subKey?.GetValue("DisplayName") as string;
                    if (string.IsNullOrWhiteSpace(name))
                    {
                        continue;
                    }

                    var isSystemComponent = subKey?.GetValue("SystemComponent") is int sc && sc == 1;
                    if (isSystemComponent)
                    {
                        continue;
                    }

                    apps.Add(new InstalledApp
                    {
                        Name = name,
                        Publisher = subKey?.GetValue("Publisher") as string,
                        Version = subKey?.GetValue("DisplayVersion") as string,
                        InstallDate = ParseInstallDate(subKey?.GetValue("InstallDate") as string),
                        EstimatedSizeMb = subKey?.GetValue("EstimatedSize") is int sizeKb ? sizeKb / 1024.0 : null,
                        UninstallCommand = subKey?.GetValue("UninstallString") as string
                    });
                }
            }

            return apps
                .GroupBy(a => a.Name, StringComparer.OrdinalIgnoreCase)
                .Select(g => g.First())
                .OrderBy(a => a.Name, StringComparer.OrdinalIgnoreCase)
                .ToList();
        }, cancellationToken);
    }

    public OperationResult LaunchUninstaller(InstalledApp app)
    {
        if (string.IsNullOrWhiteSpace(app.UninstallCommand))
        {
            return OperationResult.Fail("This application does not provide an uninstaller.");
        }

        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = $"/c {app.UninstallCommand}",
                UseShellExecute = true
            });
            return OperationResult.Ok();
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            return OperationResult.Fail($"Unable to start the uninstaller for \"{app.Name}\".", ex.Message);
        }
    }

    private static DateOnly? ParseInstallDate(string? raw)
    {
        if (raw is { Length: 8 } &&
            DateTime.TryParseExact(raw, "yyyyMMdd", CultureInfo.InvariantCulture, DateTimeStyles.None, out var date))
        {
            return DateOnly.FromDateTime(date);
        }

        return null;
    }
}
