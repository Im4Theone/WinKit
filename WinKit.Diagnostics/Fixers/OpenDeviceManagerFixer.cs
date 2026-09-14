using System.Diagnostics;
using WinKit.Core.Models;

namespace WinKit.Diagnostics.Fixers;

/// <summary>Opens the built-in Device Manager. WinKit never touches drivers directly or automatically.</summary>
public sealed class OpenDeviceManagerFixer : IDiagnosticFixer
{
    public string CheckId => DiagnosticCheckIds.Drivers;

    public Task<OperationResult<string>> FixAsync(IProgress<string>? progress, CancellationToken cancellationToken)
    {
        try
        {
            Process.Start(new ProcessStartInfo("devmgmt.msc") { UseShellExecute = true });
            return Task.FromResult(OperationResult<string>.Ok("Device Manager opened."));
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            return Task.FromResult(OperationResult<string>.Fail("Could not open Device Manager.", ex.Message));
        }
    }
}
