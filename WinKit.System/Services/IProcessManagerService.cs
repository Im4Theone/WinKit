using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public interface IProcessManagerService
{
    Task<IReadOnlyList<ProcessEntry>> GetProcessesAsync(CancellationToken cancellationToken = default);

    OperationResult EndProcess(int processId);
}
