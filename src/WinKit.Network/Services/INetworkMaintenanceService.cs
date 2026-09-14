using WinKit.Core.Models;

namespace WinKit.Network.Services;

/// <summary>
/// Destructive-ish network maintenance operations. Each maps to a single,
/// well-known Windows network command — no hidden side effects.
/// </summary>
public interface INetworkMaintenanceService
{
    Task<OperationResult> FlushDnsAsync(CancellationToken cancellationToken = default);

    Task<OperationResult> DhcpRenewAsync(CancellationToken cancellationToken = default);

    Task<OperationResult> DhcpReleaseAsync(CancellationToken cancellationToken = default);

    Task<OperationResult> ResetWinsockAsync(CancellationToken cancellationToken = default);

    Task<OperationResult> ResetTcpIpAsync(CancellationToken cancellationToken = default);
}
