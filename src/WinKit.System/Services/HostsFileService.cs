using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public sealed class HostsFileService : IHostsFileService
{
    public string HostsFilePath { get; } = Path.Combine(
        Environment.GetEnvironmentVariable("SystemRoot") ?? @"C:\Windows",
        "System32", "drivers", "etc", "hosts");

    public async Task<string> ReadRawAsync(CancellationToken cancellationToken = default)
    {
        return File.Exists(HostsFilePath)
            ? await File.ReadAllTextAsync(HostsFilePath, cancellationToken)
            : string.Empty;
    }

    public async Task<IReadOnlyList<HostsEntry>> ParseEntriesAsync(CancellationToken cancellationToken = default)
    {
        var raw = await ReadRawAsync(cancellationToken);
        var entries = new List<HostsEntry>();

        foreach (var rawLine in raw.Split('\n'))
        {
            var line = rawLine.Trim('\r', ' ', '\t');
            if (line.Length == 0)
            {
                continue;
            }

            var isEnabled = !line.StartsWith('#');
            var content = isEnabled ? line : line.TrimStart('#', ' ');

            var parts = content.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length < 2 || !System.Net.IPAddress.TryParse(parts[0], out _))
            {
                continue;
            }

            entries.Add(new HostsEntry { IpAddress = parts[0], HostName = parts[1], IsEnabled = isEnabled });
        }

        return entries;
    }

    public async Task<OperationResult> WriteRawAsync(string content, CancellationToken cancellationToken = default)
    {
        try
        {
            await File.WriteAllTextAsync(HostsFilePath, content, cancellationToken);
            return OperationResult.Ok();
        }
        catch (UnauthorizedAccessException ex)
        {
            return OperationResult.Fail(
                "Editing the hosts file requires administrator access. Restart WinKit as Administrator.", ex.Message);
        }
        catch (IOException ex)
        {
            return OperationResult.Fail("Unable to save the hosts file.", ex.Message);
        }
    }
}
