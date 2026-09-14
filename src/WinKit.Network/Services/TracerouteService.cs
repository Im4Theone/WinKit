using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using WinKit.Network.Models;
using NetPingReply = System.Net.NetworkInformation.PingReply;

namespace WinKit.Network.Services;

public sealed class TracerouteService : ITracerouteService
{
    public async Task TraceAsync(
        string host,
        IProgress<TracerouteHop> progress,
        CancellationToken cancellationToken = default,
        int maxHops = 30)
    {
        IPAddress? destination;
        try
        {
            var entry = await Dns.GetHostEntryAsync(host, cancellationToken);
            destination = entry.AddressList.FirstOrDefault();
        }
        catch (SocketException)
        {
            destination = null;
        }

        if (destination is null)
        {
            progress.Report(new TracerouteHop { HopNumber = 0, TimedOut = true });
            return;
        }

        using var ping = new Ping();
        var buffer = System.Text.Encoding.ASCII.GetBytes(new string('a', 32));

        for (var ttl = 1; ttl <= maxHops; ttl++)
        {
            cancellationToken.ThrowIfCancellationRequested();

            var options = new PingOptions(ttl, dontFragment: true);
            NetPingReply reply;
            try
            {
                reply = await ping.SendPingAsync(destination, TimeSpan.FromMilliseconds(4000), buffer, options, cancellationToken);
            }
            catch (PingException)
            {
                progress.Report(new TracerouteHop { HopNumber = ttl, TimedOut = true });
                continue;
            }

            if (reply.Status is IPStatus.TtlExpired or IPStatus.Success)
            {
                string? hostName = null;
                try
                {
                    var entry = await Dns.GetHostEntryAsync(reply.Address);
                    hostName = entry.HostName;
                }
                catch (SocketException)
                {
                    // Reverse DNS not available for this hop; address-only is fine.
                }

                var reachedDestination = reply.Status == IPStatus.Success;
                progress.Report(new TracerouteHop
                {
                    HopNumber = ttl,
                    Address = reply.Address.ToString(),
                    HostName = hostName,
                    RoundTripTimeMs = reply.RoundtripTime,
                    ReachedDestination = reachedDestination
                });

                if (reachedDestination)
                {
                    return;
                }
            }
            else
            {
                progress.Report(new TracerouteHop { HopNumber = ttl, TimedOut = true });
            }
        }
    }
}
