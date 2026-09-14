using WinKit.Network.Models;

namespace WinKit.Network.Services;

public interface IIpConfigurationService
{
    Task<IReadOnlyList<AdapterConfiguration>> GetAdaptersAsync(CancellationToken cancellationToken = default);
}
