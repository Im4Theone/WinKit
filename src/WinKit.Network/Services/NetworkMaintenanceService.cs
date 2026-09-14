using WinKit.Core.Models;
using WinKit.Infrastructure;

namespace WinKit.Network.Services;

public sealed class NetworkMaintenanceService : INetworkMaintenanceService
{
    public Task<OperationResult> FlushDnsAsync(CancellationToken cancellationToken = default) =>
        RunAsync("ipconfig", "/flushdns", elevated: false,
            "Unable to flush the DNS cache.", cancellationToken);

    public Task<OperationResult> DhcpRenewAsync(CancellationToken cancellationToken = default) =>
        RunAsync("ipconfig", "/renew", elevated: true,
            "Unable to renew the DHCP lease.", cancellationToken);

    public Task<OperationResult> DhcpReleaseAsync(CancellationToken cancellationToken = default) =>
        RunAsync("ipconfig", "/release", elevated: true,
            "Unable to release the DHCP lease.", cancellationToken);

    public Task<OperationResult> ResetWinsockAsync(CancellationToken cancellationToken = default) =>
        RunAsync("netsh", "winsock reset", elevated: true,
            "Unable to reset Winsock.", cancellationToken);

    public Task<OperationResult> ResetTcpIpAsync(CancellationToken cancellationToken = default) =>
        RunAsync("netsh", "int ip reset", elevated: true,
            "Unable to reset the TCP/IP stack.", cancellationToken);

    private static async Task<OperationResult> RunAsync(
        string fileName, string arguments, bool elevated, string failureMessage, CancellationToken cancellationToken)
    {
        try
        {
            var result = await ProcessRunner.RunAsync(fileName, arguments, cancellationToken, elevated);
            return result.Succeeded
                ? OperationResult.Ok()
                : OperationResult.Fail(failureMessage, result.StandardError);
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            // The user declined the UAC elevation prompt.
            return OperationResult.Fail(failureMessage, ex.Message);
        }
    }
}
