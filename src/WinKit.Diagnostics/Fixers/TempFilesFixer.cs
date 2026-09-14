using WinKit.Cleanup.Models;
using WinKit.Cleanup.Services;
using WinKit.Core.Models;

namespace WinKit.Diagnostics.Fixers;

/// <summary>Delegates straight to the existing Cleanup executor rather than duplicating deletion logic.</summary>
public sealed class TempFilesFixer : IDiagnosticFixer
{
    private readonly ICleanupExecutor _cleanupExecutor;

    public TempFilesFixer(ICleanupExecutor cleanupExecutor)
    {
        _cleanupExecutor = cleanupExecutor;
    }

    public string CheckId => DiagnosticCheckIds.TempFiles;

    public async Task<OperationResult<string>> FixAsync(IProgress<string>? progress, CancellationToken cancellationToken)
    {
        var cleanupProgress = progress is null ? null : new Progress<string>(progress.Report);

        var result = await _cleanupExecutor.CleanAsync(
            new[] { CleanupCategory.UserTemp, CleanupCategory.WindowsTemp },
            cleanupProgress,
            cancellationToken);

        if (!result.Success)
        {
            return OperationResult<string>.Fail(result.UserMessage ?? "Cleanup failed.", result.TechnicalDetail);
        }

        var summary = result.Value!;
        return OperationResult<string>.Ok(
            $"Removed {summary.FilesRemoved} file(s), freeing {summary.GbFreed:0.##} GB.",
            result.UserMessage);
    }
}
