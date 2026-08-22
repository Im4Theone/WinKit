using WinKit.Core.Models;
using WinKit.Network.Models;

namespace WinKit.Network.Services;

public interface IDnsLookupService
{
    Task<OperationResult<DnsLookupResult>> LookupAsync(string host, CancellationToken cancellationToken = default);
}
