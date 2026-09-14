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
            var (fileName, arguments) = SplitCommand(app.UninstallCommand);
            Process.Start(new ProcessStartInfo
            {
                FileName = fileName,
                Arguments = arguments,
                UseShellExecute = true
            });
            return OperationResult.Ok();
        }
        catch (Exception ex) when (ex is System.ComponentModel.Win32Exception or InvalidOperationException)
        {
            return OperationResult.Fail($"Unable to start the uninstaller for \"{app.Name}\".", ex.Message);
        }
    }

    /// <summary>
    /// Registry UninstallString values are a raw command line (e.g. "C:\Program Files\App\uninst.exe" /S,
    /// or MsiExec.exe /X{GUID}) rather than a pre-split file name + arguments. Shelling this through
    /// cmd.exe /c breaks on quoted paths, so split it ourselves the way Windows itself would.
    /// </summary>
    private static (string FileName, string Arguments) SplitCommand(string command)
    {
        command = command.Trim();

        if (command.StartsWith('"'))
        {
            var closingQuote = command.IndexOf('"', 1);
            if (closingQuote > 0)
            {
                var fileName = command[1..closingQuote];
                var arguments = command[(closingQuote + 1)..].Trim();
                return (fileName, arguments);
            }
        }

        var spaceIndex = command.IndexOf(' ');
        return spaceIndex < 0
            ? (command, string.Empty)
            : (command[..spaceIndex], command[(spaceIndex + 1)..].Trim());
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
