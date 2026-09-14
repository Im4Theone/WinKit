using System.Diagnostics;
using System.Security.Principal;
using WinKit.Core.Abstractions;

namespace WinKit.Infrastructure;

public sealed class ElevationService : IElevationService
{
    public bool IsElevated { get; } = ComputeIsElevated();

    public bool RelaunchElevated(string arguments)
    {
        var exePath = Environment.ProcessPath;
        if (string.IsNullOrEmpty(exePath))
        {
            return false;
        }

        try
        {
            var startInfo = new ProcessStartInfo(exePath, arguments)
            {
                UseShellExecute = true,
                Verb = "runas"
            };
            Process.Start(startInfo);
            return true;
        }
        catch (System.ComponentModel.Win32Exception)
        {
            // ERROR_CANCELLED: the user declined the UAC prompt.
            return false;
        }
    }

    private static bool ComputeIsElevated()
    {
        using var identity = WindowsIdentity.GetCurrent();
        var principal = new WindowsPrincipal(identity);
        return principal.IsInRole(WindowsBuiltInRole.Administrator);
    }
}
