using System.Net.NetworkInformation;
using System.Net.Sockets;
using WinKit.Network.Models;

namespace WinKit.Network.Services;

public sealed class IpConfigurationService : IIpConfigurationService
{
    public Task<IReadOnlyList<AdapterConfiguration>> GetAdaptersAsync(CancellationToken cancellationToken = default)
    {
        return Task.Run(() =>
        {
            IReadOnlyList<AdapterConfiguration> result = NetworkInterface.GetAllNetworkInterfaces()
                .Where(nic => nic.NetworkInterfaceType != NetworkInterfaceType.Loopback)
                .Select(nic =>
                {
                    var props = nic.GetIPProperties();
                    return new AdapterConfiguration
                    {
                        Name = nic.Name,
                        Description = nic.Description,
                        MacAddress = FormatMac(nic.GetPhysicalAddress()),
                        IPv4Addresses = props.UnicastAddresses
                            .Where(a => a.Address.AddressFamily == AddressFamily.InterNetwork)
                            .Select(a => a.Address.ToString()).ToList(),
                        IPv6Addresses = props.UnicastAddresses
                            .Where(a => a.Address.AddressFamily == AddressFamily.InterNetworkV6)
                            .Select(a => a.Address.ToString()).ToList(),
                        Gateways = props.GatewayAddresses.Select(g => g.Address.ToString()).ToList(),
                        DnsServers = props.DnsAddresses.Select(d => d.ToString()).ToList(),
                        DhcpEnabled = TryGetDhcpEnabled(props),
                        IsUp = nic.OperationalStatus == OperationalStatus.Up
                    };
                })
                .OrderByDescending(a => a.IsUp)
                .ToList();

            return result;
        }, cancellationToken);
    }

    private static bool TryGetDhcpEnabled(IPInterfaceProperties props)
    {
        try
        {
            return props.GetIPv4Properties()?.IsDhcpEnabled ?? false;
        }
        catch (NetworkInformationException)
        {
            // Some adapters (tunnels, virtual NICs) don't support IPv4 properties at all.
            return false;
        }
    }

    private static string? FormatMac(PhysicalAddress address)
    {
        var bytes = address.GetAddressBytes();
        return bytes.Length == 0 ? null : string.Join('-', bytes.Select(b => b.ToString("X2")));
    }
}
