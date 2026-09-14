using System.Net.NetworkInformation;

namespace WinKit.Network.Models;

public sealed class PingReply
{
    public required int Sequence { get; init; }
    public IPStatus Status { get; init; }
    public long RoundTripTimeMs { get; init; }
    public string? Address { get; init; }
}

public sealed class DnsLookupResult
{
    public required string HostName { get; init; }
    public IReadOnlyList<string> Addresses { get; init; } = Array.Empty<string>();
}

public sealed class TracerouteHop
{
    public required int HopNumber { get; init; }
    public string? Address { get; init; }
    public string? HostName { get; init; }
    public long? RoundTripTimeMs { get; init; }
    public bool TimedOut { get; init; }
    public bool ReachedDestination { get; init; }
}

public sealed class AdapterConfiguration
{
    public required string Name { get; init; }
    public string? Description { get; init; }
    public string? MacAddress { get; init; }
    public IReadOnlyList<string> IPv4Addresses { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> IPv6Addresses { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> Gateways { get; init; } = Array.Empty<string>();
    public IReadOnlyList<string> DnsServers { get; init; } = Array.Empty<string>();
    public bool DhcpEnabled { get; init; }
    public bool IsUp { get; init; }
}
