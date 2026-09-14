using System.ServiceProcess;
using Microsoft.Win32;
using WinKit.Core.Models;

namespace WinKit.Diagnostics.Checks;

/// <summary>
/// Checks the two things that reliably indicate a real Windows Update problem:
/// the update service being disabled outright (Stopped/Manual is entirely normal -
/// it runs on a schedule, not continuously), and a pending-reboot flag left over
/// from an already-installed update.
/// </summary>
public sealed class WindowsUpdateCheck : IDiagnosticCheck
{
    private const string RebootPendingCbsKey =
        @"SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending";

    private const string RebootPendingAuKey =
        @"SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired";

    private const string PendingFileRenameValue =
        @"SYSTEM\CurrentControlSet\Control\Session Manager";

    public string Id => DiagnosticCheckIds.WindowsUpdate;
    public string Name => "Windows Update";
    public DiagnosticCategory Category => DiagnosticCategory.WindowsUpdate;

    public Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        return Task.Run(() =>
        {
            var serviceDisabled = IsServiceDisabled("wuauserv", out var serviceDetail);
            var rebootPending = IsRebootPending();

            if (serviceDisabled)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Failed,
                    Severity = DiagnosticSeverity.High,
                    Summary = "The Windows Update service is disabled.",
                    TechnicalDetail = serviceDetail,
                    RecommendedAction = "Enable and start the Windows Update service.",
                    FixAction = new DiagnosticFixAction
                    {
                        Label = "Enable",
                        Kind = DiagnosticActionKind.Fix,
                        RequiresElevation = true,
                        ConfirmationMessage = "This sets the Windows Update service back to its default startup " +
                                              "type and starts it. It requires administrator permission."
                    }
                };
            }

            if (rebootPending)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Informational,
                    Severity = DiagnosticSeverity.Low,
                    Summary = "A restart is required to finish applying recent changes.",
                    TechnicalDetail = "A pending-reboot flag is set. This is most often left by Windows Update, " +
                                      "but can also be left by driver or application installers.",
                    RecommendedAction = "Restart your PC when convenient."
                };
            }

            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.Passed,
                Summary = "Windows Update is configured normally and no restart is pending."
            };
        }, cancellationToken);
    }

    private static bool IsServiceDisabled(string serviceName, out string detail)
    {
        try
        {
            using var service = new ServiceController(serviceName);
            detail = $"{serviceName}: start type = {service.StartType}, status = {service.Status}.";
            return service.StartType == ServiceStartMode.Disabled;
        }
        catch (InvalidOperationException)
        {
            detail = $"Could not query the {serviceName} service.";
            return false;
        }
    }

    private static bool IsRebootPending()
    {
        return KeyExists(Registry.LocalMachine, RebootPendingCbsKey)
            || KeyExists(Registry.LocalMachine, RebootPendingAuKey)
            || ValueExists(Registry.LocalMachine, PendingFileRenameValue, "PendingFileRenameOperations");
    }

    private static bool KeyExists(RegistryKey hive, string path)
    {
        using var key = hive.OpenSubKey(path);
        return key is not null;
    }

    private static bool ValueExists(RegistryKey hive, string path, string valueName)
    {
        using var key = hive.OpenSubKey(path);
        return key?.GetValue(valueName) is not null;
    }
}
