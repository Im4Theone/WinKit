using System.Management;
using System.Net.NetworkInformation;
using Microsoft.Win32;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public sealed class SystemInfoService : ISystemInfoService
{
    public Task<SystemOverview> GetOverviewAsync(CancellationToken cancellationToken = default)
    {
        return Task.Run(() =>
        {
            var overview = new SystemOverview
            {
                Cpu = GetCpuInfo(),
                Memory = GetMemoryInfo(),
                Gpu = GetGpuInfo(),
                Storage = GetStorageInfo(),
                Network = GetNetworkInfo(),
                WindowsVersion = GetWindowsVersion(),
                Uptime = TimeSpan.FromMilliseconds(Environment.TickCount64)
            };
            return overview;
        }, cancellationToken);
    }

    private static CpuInfo GetCpuInfo()
    {
        string name = "Unknown processor";
        double usage = 0;
        int coreCount = Environment.ProcessorCount;

        try
        {
            using var searcher = new ManagementObjectSearcher("SELECT Name, LoadPercentage FROM Win32_Processor");
            foreach (ManagementObject item in searcher.Get())
            {
                name = item["Name"]?.ToString()?.Trim() ?? name;
                if (item["LoadPercentage"] is not null)
                {
                    usage = Convert.ToDouble(item["LoadPercentage"]);
                }
                break;
            }
        }
        catch (ManagementException)
        {
            // WMI unavailable; fall back to defaults.
        }

        return new CpuInfo { Name = name, UsagePercent = usage, CoreCount = coreCount };
    }

    private static MemoryInfo GetMemoryInfo()
    {
        try
        {
            using var searcher = new ManagementObjectSearcher(
                "SELECT TotalVisibleMemorySize, FreePhysicalMemory FROM Win32_OperatingSystem");
            foreach (ManagementObject item in searcher.Get())
            {
                var totalKb = Convert.ToDouble(item["TotalVisibleMemorySize"]);
                var freeKb = Convert.ToDouble(item["FreePhysicalMemory"]);
                var totalGb = totalKb / 1024.0 / 1024.0;
                var usedGb = (totalKb - freeKb) / 1024.0 / 1024.0;
                return new MemoryInfo { TotalGb = totalGb, UsedGb = usedGb };
            }
        }
        catch (ManagementException)
        {
            // WMI unavailable; fall back to defaults.
        }

        return new MemoryInfo();
    }

    private static GpuInfo GetGpuInfo()
    {
        try
        {
            using var searcher = new ManagementObjectSearcher("SELECT Name, AdapterRAM FROM Win32_VideoController");
            foreach (ManagementObject item in searcher.Get())
            {
                var name = item["Name"]?.ToString()?.Trim() ?? "Unknown GPU";
                double? memGb = null;
                if (item["AdapterRAM"] is not null)
                {
                    var bytes = Convert.ToInt64(Convert.ToUInt32(item["AdapterRAM"]));
                    if (bytes > 0)
                    {
                        memGb = Math.Round(bytes / 1024.0 / 1024.0 / 1024.0, 1);
                    }
                }

                return new GpuInfo { Name = name, DedicatedMemoryGb = memGb };
            }
        }
        catch (ManagementException)
        {
            // WMI unavailable; fall back to defaults.
        }

        return new GpuInfo();
    }

    private static IReadOnlyList<StorageVolumeInfo> GetStorageInfo()
    {
        var volumes = new List<StorageVolumeInfo>();
        foreach (var drive in DriveInfo.GetDrives())
        {
            if (drive.DriveType != DriveType.Fixed || !drive.IsReady)
            {
                continue;
            }

            volumes.Add(new StorageVolumeInfo
            {
                Name = drive.Name.TrimEnd('\\'),
                TotalGb = drive.TotalSize / 1024.0 / 1024.0 / 1024.0,
                FreeGb = drive.AvailableFreeSpace / 1024.0 / 1024.0 / 1024.0
            });
        }

        return volumes;
    }

    private static NetworkInfo GetNetworkInfo()
    {
        var candidate = NetworkInterface.GetAllNetworkInterfaces()
            .Where(nic => nic.OperationalStatus == OperationalStatus.Up
                          && nic.NetworkInterfaceType != NetworkInterfaceType.Loopback
                          && nic.NetworkInterfaceType != NetworkInterfaceType.Tunnel)
            .OrderByDescending(nic => nic.Speed)
            .FirstOrDefault();

        if (candidate is null)
        {
            return new NetworkInfo { AdapterName = "Not connected", IsConnected = false };
        }

        var ip = candidate.GetIPProperties().UnicastAddresses
            .FirstOrDefault(a => a.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
            ?.Address.ToString();

        return new NetworkInfo
        {
            AdapterName = candidate.Name,
            IsConnected = true,
            IpAddress = ip
        };
    }

    private static string GetWindowsVersion()
    {
        try
        {
            using var key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Microsoft\Windows NT\CurrentVersion");
            var productName = key?.GetValue("ProductName") as string ?? "Windows";
            var displayVersion = key?.GetValue("DisplayVersion") as string;
            var buildNumber = key?.GetValue("CurrentBuildNumber") as string;

            // Windows 11 still reports "Windows 10 ..." for ProductName in the registry;
            // build 22000+ is the only reliable way to tell them apart.
            if (int.TryParse(buildNumber, out var build) && build >= 22000)
            {
                productName = productName.Replace("Windows 10", "Windows 11", StringComparison.Ordinal);
            }

            return string.IsNullOrEmpty(displayVersion)
                ? $"{productName} (Build {buildNumber})"
                : $"{productName} {displayVersion} (Build {buildNumber})";
        }
        catch (Exception ex) when (ex is System.Security.SecurityException or ObjectDisposedException)
        {
            return Environment.OSVersion.VersionString;
        }
    }
}
