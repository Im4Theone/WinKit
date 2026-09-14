using WinKit.Cleanup.Models;
using WinKit.Core.Models;

namespace WinKit.Cleanup.Services;

public interface ICleanupExecutor
{
    Task<OperationResult<CleanupSummary>> CleanAsync(
        IReadOnlyCollection<CleanupCategory> categories,
        IProgress<string>? progress = null,
        CancellationToken cancellationToken = default);
}
