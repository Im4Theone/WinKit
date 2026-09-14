using System.Net.NetworkInformation;
using WinKit.Network.Models;
using WinKitPingReply = WinKit.Network.Models.PingReply;

namespace WinKit.Network.Services;

public sealed class PingService : IPingService
{
    public async Task PingAsync(
        string host,
        int count,
        IProgress<WinKitPingReply> progress,
        CancellationToken cancellationToken = default)
    {
        using var ping = new Ping();
        var options = new PingOptions { Ttl = 64 };
        var buffer = System.Text.Encoding.ASCII.GetBytes(new string('a', 32));

        for (var i = 1; i <= count; i++)
        {
            cancellationToken.ThrowIfCancellationRequested();

            try
            {
                var reply = await ping.SendPingAsync(host, TimeSpan.FromMilliseconds(4000), buffer, options, cancellationToken);
                progress.Report(new WinKitPingReply
                {
                    Sequence = i,
                    Status = reply.Status,
                    RoundTripTimeMs = reply.Status == IPStatus.Success ? reply.RoundtripTime : 0,
                    Address = reply.Address?.ToString()
                });
            }
            catch (PingException)
            {
                progress.Report(new WinKitPingReply { Sequence = i, Status = IPStatus.Unknown });
            }

            if (i < count)
            {
                await Task.Delay(1000, cancellationToken);
            }
        }
    }
}
