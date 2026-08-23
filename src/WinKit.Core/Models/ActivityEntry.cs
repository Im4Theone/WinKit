namespace WinKit.Core.Models;

public enum ActivityKind
{
    Info,
    Success,
    Warning,
    Error
}

public enum ActivityCategory
{
    /// <summary>Something WinKit did — diagnostics runs, cleanup, network maintenance, updates.</summary>
    Action,

    /// <summary>Theme and app-setting changes.</summary>
    ThemeAndSettings
}

public sealed class ActivityEntry
{
    public required string Title { get; init; }
    public string? Detail { get; init; }
    public ActivityKind Kind { get; init; } = ActivityKind.Info;
    public ActivityCategory Category { get; init; } = ActivityCategory.Action;
    public DateTimeOffset Timestamp { get; init; } = DateTimeOffset.Now;
}
