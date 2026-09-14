namespace WinKit.Core.Models;

public enum DiagnosticStatus
{
    Passed,
    Informational,
    Warning,
    Failed,

    /// <summary>The signal this check looks for doesn't exist on this system (e.g. no SMART support).</summary>
    NotApplicable
}

/// <summary>How much a Warning/Failed result actually matters. Left at None for anything that isn't a problem.</summary>
public enum DiagnosticSeverity
{
    None,
    Low,
    Medium,
    High
}

public enum DiagnosticCategory
{
    WindowsIntegrity,
    WindowsUpdate,
    Drivers,
    Startup,
    Services,
    Storage,
    Network
}

public enum DiagnosticActionKind
{
    /// <summary>The diagnostics engine performs this action itself via a matching IDiagnosticFixer.</summary>
    Fix,

    /// <summary>The UI sends the user to an existing WinKit tool to act manually (no automatic change).</summary>
    Review
}

/// <summary>The action offered alongside a check result. Always requires explicit user confirmation before running.</summary>
public sealed class DiagnosticFixAction
{
    public required string Label { get; init; }
    public DiagnosticActionKind Kind { get; init; } = DiagnosticActionKind.Fix;
    public required string ConfirmationMessage { get; init; }
    public bool IsDestructive { get; init; }
    public bool RequiresElevation { get; init; }
}

public sealed class DiagnosticCheckResult
{
    /// <summary>Stable identifier matching an IDiagnosticCheck.Id, used to route fix/review actions.</summary>
    public required string CheckId { get; init; }
    public required string Name { get; init; }
    public required DiagnosticCategory Category { get; init; }
    public DiagnosticStatus Status { get; init; }
    public DiagnosticSeverity Severity { get; init; } = DiagnosticSeverity.None;
    public required string Summary { get; init; }
    public string? TechnicalDetail { get; init; }
    public string? RecommendedAction { get; init; }
    public DiagnosticFixAction? FixAction { get; init; }
}

/// <summary>
/// A lightweight snapshot of the last completed full scan - just enough for a
/// cheap "last known state" display (e.g. the Dashboard) without holding onto
/// or re-deriving the full per-check result list.
/// </summary>
public sealed class DiagnosticsSummary
{
    public required int TotalChecks { get; init; }
    public required int IssueCount { get; init; }
    public required DateTimeOffset RanAt { get; init; }
}

/// <summary>Stable check identifiers shared between WinKit.Diagnostics (checks/fixers) and the UI (action routing).</summary>
public static class DiagnosticCheckIds
{
    public const string WindowsIntegrity = "windows-integrity";
    public const string WindowsUpdate = "windows-update";
    public const string Drivers = "drivers";
    public const string Startup = "startup";
    public const string Services = "services";
    public const string DiskSpace = "disk-space";
    public const string TempFiles = "temp-files";
    public const string DiskHealth = "disk-health";
    public const string InternetConnectivity = "internet-connectivity";
    public const string DnsResolution = "dns-resolution";
    public const string NetworkAdapter = "network-adapter";
}
