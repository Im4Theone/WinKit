namespace WinKit.Core.Models;

public enum ActivityKind
{
    Info,
    Success,
    Warning,
    Error
}

public sealed class ActivityEntry
{
    public required string Title { get; init; }
    public string? Detail { get; init; }
    public ActivityKind Kind { get; init; } = ActivityKind.Info;
    public DateTimeOffset Timestamp { get; init; } = DateTimeOffset.Now;
}
