using WinKit.Network.Models;

namespace WinKit.Network.Services;

public interface IPingService
{
    Task PingAsync(
        string host,
        int count,
        IProgress<PingReply> progress,
        CancellationToken cancellationToken = default);
}
