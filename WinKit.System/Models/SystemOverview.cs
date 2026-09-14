namespace WinKit.SystemTools.Models;

public sealed class CpuInfo
{
    public string Name { get; init; } = "Unknown";
    public double UsagePercent { get; init; }
    public int CoreCount { get; init; }
}

public sealed class MemoryInfo
{
    public double TotalGb { get; init; }
    public double UsedGb { get; init; }
    public double UsagePercent => TotalGb <= 0 ? 0 : UsedGb / TotalGb * 100.0;
}

public sealed class GpuInfo
{
    public string Name { get; init; } = "Unknown";
    public double? DedicatedMemoryGb { get; init; }
}

public sealed class StorageVolumeInfo
{
    public required string Name { get; init; }
    public double TotalGb { get; init; }
    public double FreeGb { get; init; }
    public double UsedGb => TotalGb - FreeGb;
    public double UsagePercent => TotalGb <= 0 ? 0 : UsedGb / TotalGb * 100.0;
}

public sealed class NetworkInfo
{
    public string AdapterName { get; init; } = "Not connected";
    public bool IsConnected { get; init; }
    public string? IpAddress { get; init; }
}

public sealed class SystemOverview
{
    public required CpuInfo Cpu { get; init; }
    public required MemoryInfo Memory { get; init; }
    public required GpuInfo Gpu { get; init; }
    public required IReadOnlyList<StorageVolumeInfo> Storage { get; init; }
    public required NetworkInfo Network { get; init; }
    public required string WindowsVersion { get; init; }
    public required TimeSpan Uptime { get; init; }
}
