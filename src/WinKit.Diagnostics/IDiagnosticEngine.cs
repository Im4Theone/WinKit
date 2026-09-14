using WinKit.Core.Models;

namespace WinKit.Diagnostics;

public interface IDiagnosticEngine
{
    /// <summary>Runs every registered check. Individual check failures/timeouts never abort the run.</summary>
    Task<IReadOnlyList<DiagnosticCheckResult>> RunAllAsync(
        IProgress<DiagnosticCheckResult>? progress = null,
        CancellationToken cancellationToken = default);

    /// <summary>Runs the fixer registered for the given check id, if any.</summary>
    Task<OperationResult<string>> FixAsync(
        string checkId,
        IProgress<string>? progress = null,
        CancellationToken cancellationToken = default);
}
