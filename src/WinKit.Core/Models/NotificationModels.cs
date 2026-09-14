namespace WinKit.Core.Models;

public enum NotificationSeverity
{
    Success,
    Error,
    Warning,
    Update,
    Info
}

public sealed class NotificationRequest
{
    public required string Title { get; init; }
    public string? Description { get; init; }
    public NotificationSeverity Severity { get; init; } = NotificationSeverity.Info;
    public string? ActionText { get; init; }
    public Action? Action { get; init; }
    public TimeSpan? AutoDismissAfter { get; init; } = TimeSpan.FromSeconds(3);
}
