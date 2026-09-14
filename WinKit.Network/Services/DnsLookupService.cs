using System.Net;
using System.Net.Sockets;
using WinKit.Core.Models;
using WinKit.Network.Models;

namespace WinKit.Network.Services;

public sealed class DnsLookupService : IDnsLookupService
{
    public async Task<OperationResult<DnsLookupResult>> LookupAsync(
        string host, CancellationToken cancellationToken = default)
    {
        try
        {
            var entry = await Dns.GetHostEntryAsync(host, cancellationToken);
            var addresses = entry.AddressList
                .Where(a => a.AddressFamily is AddressFamily.InterNetwork or AddressFamily.InterNetworkV6)
                .Select(a => a.ToString())
                .ToList();

            if (addresses.Count == 0)
            {
                return OperationResult<DnsLookupResult>.Fail($"No records found for \"{host}\".");
            }

            return OperationResult<DnsLookupResult>.Ok(new DnsLookupResult
            {
                HostName = entry.HostName,
                Addresses = addresses
            });
        }
        catch (SocketException ex)
        {
            return OperationResult<DnsLookupResult>.Fail($"Could not resolve \"{host}\".", ex.Message);
        }
    }
}
