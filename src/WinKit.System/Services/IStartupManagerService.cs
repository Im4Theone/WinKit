using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public interface IStartupManagerService
{
    Task<IReadOnlyList<StartupEntry>> GetEntriesAsync(CancellationToken cancellationToken = default);

    Task<OperationResult> SetEnabledAsync(StartupEntry entry, bool enabled, CancellationToken cancellationToken = default);
}
