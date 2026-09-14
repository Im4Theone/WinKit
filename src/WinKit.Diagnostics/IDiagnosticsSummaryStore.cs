using WinKit.Core.Models;

namespace WinKit.Diagnostics;

/// <summary>
/// Holds the outcome of the last completed full scan in memory, so pages other
/// than Diagnostics (e.g. the Dashboard) can show a "last known" status without
/// running any checks themselves. Populated by DiagnosticEngine; never runs a
/// scan on its own.
/// </summary>
public interface IDiagnosticsSummaryStore
{
    /// <summary>Null until the first full scan of this app session completes.</summary>
    DiagnosticsSummary? LastSummary { get; }

    event EventHandler? Changed;
}
