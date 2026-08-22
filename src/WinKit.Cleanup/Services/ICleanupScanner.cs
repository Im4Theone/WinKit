using WinKit.Cleanup.Models;

namespace WinKit.Cleanup.Services;

public interface ICleanupScanner
{
    Task<IReadOnlyList<CleanupCategoryScan>> ScanAsync(CancellationToken cancellationToken = default);
}
