namespace WinKit.SystemTools.Models;

public sealed class HostsEntry
{
    public required string IpAddress { get; init; }
    public required string HostName { get; init; }
    public bool IsEnabled { get; init; } = true;
}
