using WinKit.Core.Models;
using WinKit.SystemTools.Services;

namespace WinKit.Diagnostics.Checks;

/// <summary>
/// Flags an unusually large number of enabled startup entries. Per-app "impact"
/// scoring (like Task Manager's) needs ETW boot telemetry this process doesn't
/// have access to, so this deliberately sticks to a count-based signal it can
/// actually stand behind, rather than fabricating a precision it doesn't have.
/// </summary>
public sealed class StartupCheck : IDiagnosticCheck
{
    private const int WarningThreshold = 8;

    private readonly IStartupManagerService _startupManagerService;

    public StartupCheck(IStartupManagerService startupManagerService)
    {
        _startupManagerService = startupManagerService;
    }

    public string Id => DiagnosticCheckIds.Startup;
    public string Name => "Startup Programs";
    public DiagnosticCategory Category => DiagnosticCategory.Startup;

    public async Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        var entries = await _startupManagerService.GetEntriesAsync(cancellationToken);
        var enabled = entries.Where(e => e.IsEnabled).ToList();

        if (enabled.Count > WarningThreshold)
        {
            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.Warning,
                Severity = DiagnosticSeverity.Low,
                Summary = $"{enabled.Count} programs are set to launch at startup.",
                TechnicalDetail = string.Join(Environment.NewLine, enabled.Select(e => e.Name)),
                RecommendedAction = "Review your startup programs and disable the ones you don't need running all the time.",
                FixAction = new DiagnosticFixAction
                {
                    Label = "Review Startup Items",
                    Kind = DiagnosticActionKind.Review,
                    ConfirmationMessage = "This opens Startup Manager, where you can disable individual entries."
                }
            };
        }

        return new DiagnosticCheckResult
        {
            CheckId = Id,
            Name = Name,
            Category = Category,
            Status = DiagnosticStatus.Passed,
            Summary = $"{enabled.Count} program(s) launch at startup - a normal amount."
        };
    }
}
