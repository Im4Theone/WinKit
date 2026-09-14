using WinKit.Network.Models;

namespace WinKit.Network.Services;

public interface ITracerouteService
{
    Task TraceAsync(
        string host,
        IProgress<TracerouteHop> progress,
        CancellationToken cancellationToken = default,
        int maxHops = 30);
}
