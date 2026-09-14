namespace WinKit.SystemTools.Models;

public sealed class ServiceEntry
{
    public required string Name { get; init; }
    public required string DisplayName { get; init; }
    public required string Status { get; init; }
    public required string StartupType { get; init; }
    public bool CanStart { get; init; }
    public bool CanStop { get; init; }
}
