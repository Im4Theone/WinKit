namespace WinKit.Core.Models;

public enum DiagnosticStatus
{
    Ok,
    Warning,
    Error
}

public sealed class DiagnosticCheckResult
{
    public required string Name { get; init; }
    public DiagnosticStatus Status { get; init; }
    public required string Summary { get; init; }
    public string? Explanation { get; init; }
    public string? RecommendedAction { get; init; }
}
