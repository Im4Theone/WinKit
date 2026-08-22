using Microsoft.Win32;
using WinKit.Core.Models;
using WinKit.Infrastructure;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

/// <summary>
/// Disabling a registry-based startup entry removes it from the Run key and
/// keeps a local backup so it can be restored exactly; disabling a shortcut
/// renames it in place with a ".disabled" suffix. Both are fully reversible
/// and leave no hidden state Windows itself doesn't already understand.
/// </summary>
public sealed class StartupManagerService : IStartupManagerService
{
    private const string RunKeyPath = @"Software\Microsoft\Windows\CurrentVersion\Run";

    private sealed class DisabledRegistryEntry
    {
        public required string Name { get; set; }
        public required string Command { get; set; }
        public required bool IsMachineScope { get; set; }
    }

    private static string BackupFile => Path.Combine(AppPaths.RootDirectory, "disabled-startup.json");

    public async Task<IReadOnlyList<StartupEntry>> GetEntriesAsync(CancellationToken cancellationToken = default)
    {
        var entries = new List<StartupEntry>();
        var disabled = await LoadBackupAsync();

        AddRegistryEntries(entries, Registry.CurrentUser, StartupScope.CurrentUserRegistry);
        AddRegistryEntries(entries, Registry.LocalMachine, StartupScope.AllUsersRegistry);

        foreach (var backup in disabled)
        {
            entries.Add(new StartupEntry
            {
                Name = backup.Name,
                Command = backup.Command,
                Scope = backup.IsMachineScope ? StartupScope.AllUsersRegistry : StartupScope.CurrentUserRegistry,
                IsEnabled = false
            });
        }

        AddFolderEntries(entries, Environment.SpecialFolder.Startup, StartupScope.CurrentUserFolder);
        AddFolderEntries(entries, Environment.SpecialFolder.CommonStartup, StartupScope.AllUsersFolder);

        return entries.OrderBy(e => e.Name, StringComparer.OrdinalIgnoreCase).ToList();
    }

    public async Task<OperationResult> SetEnabledAsync(
        StartupEntry entry, bool enabled, CancellationToken cancellationToken = default)
    {
        try
        {
            return entry.Scope switch
            {
                StartupScope.CurrentUserRegistry or StartupScope.AllUsersRegistry =>
                    await SetRegistryEnabledAsync(entry, enabled),
                StartupScope.CurrentUserFolder or StartupScope.AllUsersFolder =>
                    SetFolderEnabled(entry, enabled),
                _ => OperationResult.Fail("Unknown startup entry type.")
            };
        }
        catch (UnauthorizedAccessException ex)
        {
            return OperationResult.Fail(
                "This startup item requires administrator access to change. Restart WinKit as Administrator.",
                ex.Message);
        }
        catch (System.Security.SecurityException ex)
        {
            return OperationResult.Fail(
                "This startup item requires administrator access to change. Restart WinKit as Administrator.",
                ex.Message);
        }
    }

    private async Task<OperationResult> SetRegistryEnabledAsync(StartupEntry entry, bool enabled)
    {
        var isMachineScope = entry.Scope == StartupScope.AllUsersRegistry;
        var hive = isMachineScope ? Registry.LocalMachine : Registry.CurrentUser;

        if (!enabled)
        {
            using var key = hive.OpenSubKey(RunKeyPath, writable: true);
            var value = key?.GetValue(entry.Name) as string ?? entry.Command ?? string.Empty;
            key?.DeleteValue(entry.Name, throwOnMissingValue: false);

            var backups = (await LoadBackupAsync()).ToList();
            backups.RemoveAll(b => b.Name == entry.Name && b.IsMachineScope == isMachineScope);
            backups.Add(new DisabledRegistryEntry { Name = entry.Name, Command = value, IsMachineScope = isMachineScope });
            await SaveBackupAsync(backups);
        }
        else
        {
            using var key = hive.OpenSubKey(RunKeyPath, writable: true) ?? hive.CreateSubKey(RunKeyPath);
            key.SetValue(entry.Name, entry.Command ?? string.Empty);

            var backups = (await LoadBackupAsync()).ToList();
            backups.RemoveAll(b => b.Name == entry.Name && b.IsMachineScope == isMachineScope);
            await SaveBackupAsync(backups);
        }

        return OperationResult.Ok();
    }

    private static OperationResult SetFolderEnabled(StartupEntry entry, bool enabled)
    {
        if (entry.Command is null || !File.Exists(entry.Command))
        {
            return OperationResult.Fail("That shortcut could not be found.");
        }

        var target = enabled
            ? entry.Command.Replace(".disabled", string.Empty, StringComparison.OrdinalIgnoreCase)
            : entry.Command + ".disabled";

        File.Move(entry.Command, target, overwrite: false);
        return OperationResult.Ok();
    }

    private static void AddRegistryEntries(List<StartupEntry> entries, RegistryKey hive, StartupScope scope)
    {
        using var key = hive.OpenSubKey(RunKeyPath);
        if (key is null)
        {
            return;
        }

        foreach (var name in key.GetValueNames())
        {
            if (key.GetValue(name) is string command)
            {
                entries.Add(new StartupEntry { Name = name, Command = command, Scope = scope, IsEnabled = true });
            }
        }
    }

    private static void AddFolderEntries(List<StartupEntry> entries, Environment.SpecialFolder folder, StartupScope scope)
    {
        var path = Environment.GetFolderPath(folder);
        if (!Directory.Exists(path))
        {
            return;
        }

        foreach (var file in Directory.EnumerateFiles(path))
        {
            var isDisabled = file.EndsWith(".disabled", StringComparison.OrdinalIgnoreCase);
            var name = Path.GetFileNameWithoutExtension(isDisabled ? file[..^".disabled".Length] : file);
            entries.Add(new StartupEntry { Name = name, Command = file, Scope = scope, IsEnabled = !isDisabled });
        }
    }

    private static async Task<List<DisabledRegistryEntry>> LoadBackupAsync() =>
        await JsonFileStore.ReadAsync<List<DisabledRegistryEntry>>(BackupFile) ?? new List<DisabledRegistryEntry>();

    private static Task SaveBackupAsync(List<DisabledRegistryEntry> backups) =>
        JsonFileStore.WriteAsync(BackupFile, backups);
}
