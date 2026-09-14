using WinKit.Core.Models;
using WinKit.Infrastructure;

namespace WinKit.Diagnostics.Fixers;

/// <summary>
/// Restores the Windows Update service to its default (Manual) startup type and starts it.
/// Uses sc.exe (the same standard, well-known tool netsh/ipconfig-style fixes elsewhere in
/// WinKit rely on) rather than touching the registry directly.
/// </summary>
public sealed class WindowsUpdateServiceFixer : IDiagnosticFixer
{
    public string CheckId => DiagnosticCheckIds.WindowsUpdate;

    public async Task<OperationResult<string>> FixAsync(IProgress<string>? progress, CancellationToken cancellationToken)
    {
        progress?.Report("Setting the Windows Update service to start automatically...");

        try
        {
            var configResult = await ProcessRunner.RunAsync(
                "sc.exe", "config wuauserv start= demand", cancellationToken, elevated: true);

            if (configResult.ExitCode != 0)
            {
                return OperationResult<string>.Fail(
                    "Could not change the Windows Update service's startup type.", configResult.StandardError);
            }

            progress?.Report("Starting the Windows Update service...");
            var startResult = await ProcessRunner.RunAsync("sc.exe", "start wuauserv", cancellationToken, elevated: true);

            // Exit code 1056 = "service is already running", which is fine.
            if (startResult.ExitCode != 0 && startResult.ExitCode != 1056)
            {
                return OperationResult<string>.Fail(
                    "The service's startup type was fixed, but it could not be started.", startResult.StandardError);
            }

            return OperationResult<string>.Ok("The Windows Update service is enabled and running.");
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            return OperationResult<string>.Fail("The elevation request was cancelled or failed.", ex.Message);
        }
    }
}
