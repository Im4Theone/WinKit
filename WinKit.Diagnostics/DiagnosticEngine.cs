using Microsoft.Extensions.Logging;
using WinKit.Core.Models;

namespace WinKit.Diagnostics;

/// <summary>
/// Runs the registered IDiagnosticCheck instances one at a time, reporting each
/// result as it completes. A check that throws or exceeds its timeout is turned
/// into an informational result instead of aborting the rest of the scan.
/// </summary>
public sealed class DiagnosticEngine : IDiagnosticEngine
{
    private static readonly TimeSpan CheckTimeout = TimeSpan.FromSeconds(20);

    private readonly IReadOnlyList<IDiagnosticCheck> _checks;
    private readonly IReadOnlyList<IDiagnosticFixer> _fixers;
    private readonly DiagnosticsSummaryStore _summaryStore;
    private readonly ILogger<DiagnosticEngine> _logger;

    public DiagnosticEngine(
        IEnumerable<IDiagnosticCheck> checks,
        IEnumerable<IDiagnosticFixer> fixers,
        DiagnosticsSummaryStore summaryStore,
        ILogger<DiagnosticEngine> logger)
    {
        _checks = checks.ToList();
        _fixers = fixers.ToList();
        _summaryStore = summaryStore;
        _logger = logger;
    }

    public async Task<IReadOnlyList<DiagnosticCheckResult>> RunAllAsync(
        IProgress<DiagnosticCheckResult>? progress = null,
        CancellationToken cancellationToken = default)
    {
        var results = new List<DiagnosticCheckResult>(_checks.Count);

        foreach (var check in _checks)
        {
            cancellationToken.ThrowIfCancellationRequested();

            var result = await RunSingleCheckAsync(check, cancellationToken);
            results.Add(result);
            progress?.Report(result);
        }

        // Only a full, uncancelled pass updates the cached summary other pages read from.
        _summaryStore.Update(new DiagnosticsSummary
        {
            TotalChecks = results.Count,
            IssueCount = results.Count(r => r.Status is DiagnosticStatus.Warning or DiagnosticStatus.Failed),
            RanAt = DateTimeOffset.Now
        });

        return results;
    }

    public async Task<OperationResult<string>> FixAsync(
        string checkId,
        IProgress<string>? progress = null,
        CancellationToken cancellationToken = default)
    {
        var fixer = _fixers.FirstOrDefault(f => f.CheckId == checkId);
        if (fixer is null)
        {
            _logger.LogWarning("No fixer registered for check {CheckId}", checkId);
            return OperationResult<string>.Fail("No automatic fix is available for this check.");
        }

        try
        {
            return await fixer.FixAsync(progress, cancellationToken);
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Fix for check {CheckId} threw an exception", checkId);
            return OperationResult<string>.Fail("The fix failed unexpectedly.", ex.ToString());
        }
    }

    private async Task<DiagnosticCheckResult> RunSingleCheckAsync(IDiagnosticCheck check, CancellationToken cancellationToken)
    {
        using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeoutCts.CancelAfter(CheckTimeout);

        try
        {
            return await check.RunAsync(timeoutCts.Token);
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            // The user cancelled the whole scan; let it propagate so RunAllAsync stops.
            throw;
        }
        catch (OperationCanceledException)
        {
            _logger.LogWarning("Diagnostic check {CheckId} timed out after {Timeout}", check.Id, CheckTimeout);
            return new DiagnosticCheckResult
            {
                CheckId = check.Id,
                Name = check.Name,
                Category = check.Category,
                Status = DiagnosticStatus.Informational,
                Summary = "This check took too long to respond and was skipped."
            };
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Diagnostic check {CheckId} threw an exception", check.Id);
            return new DiagnosticCheckResult
            {
                CheckId = check.Id,
                Name = check.Name,
                Category = check.Category,
                Status = DiagnosticStatus.Informational,
                Summary = "This check could not complete due to an unexpected error.",
                TechnicalDetail = ex.Message
            };
        }
    }
}
