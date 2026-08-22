using WinKit.Core.Models;

namespace WinKit.Diagnostics;

public interface IDiagnosticsRunner
{
    Task<IReadOnlyList<DiagnosticCheckResult>> RunAllAsync(
        IProgress<DiagnosticCheckResult>? progress = null,
        CancellationToken cancellationToken = default);
}
