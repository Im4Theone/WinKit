using WinKit.Core.Models;

namespace WinKit.Diagnostics;

/// <summary>
/// A single, independent diagnostic check. Implementations must be self-contained:
/// no dependency on other checks, no UI, and no side effects on the system being
/// inspected (that's what IDiagnosticFixer is for).
/// </summary>
public interface IDiagnosticCheck
{
    /// <summary>Stable identifier, matched against DiagnosticCheckIds and used to route fixes.</summary>
    string Id { get; }

    string Name { get; }

    DiagnosticCategory Category { get; }

    Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken);
}
