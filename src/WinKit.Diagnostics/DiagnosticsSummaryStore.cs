using WinKit.Core.Models;

namespace WinKit.Diagnostics;

/// <summary>
/// The write side lives on the concrete type (only DiagnosticEngine takes a
/// dependency on it); everyone else depends on the read-only IDiagnosticsSummaryStore.
/// </summary>
public sealed class DiagnosticsSummaryStore : IDiagnosticsSummaryStore
{
    public DiagnosticsSummary? LastSummary { get; private set; }

    public event EventHandler? Changed;

    public void Update(DiagnosticsSummary summary)
    {
        LastSummary = summary;
        Changed?.Invoke(this, EventArgs.Empty);
    }
}
