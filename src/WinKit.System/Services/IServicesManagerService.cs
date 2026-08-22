using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public interface IServicesManagerService
{
    Task<IReadOnlyList<ServiceEntry>> GetServicesAsync(CancellationToken cancellationToken = default);

    Task<OperationResult> StartAsync(string serviceName, CancellationToken cancellationToken = default);

    Task<OperationResult> StopAsync(string serviceName, CancellationToken cancellationToken = default);
}
