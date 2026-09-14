using CommunityToolkit.Mvvm.ComponentModel;
using WinKit.Core.Models;

namespace WinKit.UI.ViewModels;

public sealed partial class DiagnosticCheckItemViewModel : ObservableObject
{
    public DiagnosticCheckItemViewModel(DiagnosticCheckResult result)
    {
        Result = result;
    }

    public DiagnosticCheckResult Result { get; }

    [ObservableProperty]
    private bool _isBusy;

    [ObservableProperty]
    private string? _statusMessage;

    public bool IsIssue => Result.Status is DiagnosticStatus.Warning or DiagnosticStatus.Failed;

    /// <summary>
    /// Sort key (ascending = most important first) so the checks that actually need
    /// attention rise to the top of each category instead of sitting in whatever
    /// order the checks happened to run in. This is the only place Severity feeds
    /// into the UI - it's not otherwise displayed, just used to rank within a status.
    /// </summary>
    public int SortPriority => Result.Status switch
    {
        DiagnosticStatus.Failed => 0,
        DiagnosticStatus.Warning => 1,
        DiagnosticStatus.Informational => 2,
        DiagnosticStatus.Passed => 3,
        DiagnosticStatus.NotApplicable => 4,
        _ => 5
    };

    public string CategoryLabel => Result.Category switch
    {
        DiagnosticCategory.WindowsIntegrity => "Windows Integrity",
        DiagnosticCategory.WindowsUpdate => "Windows Update",
        DiagnosticCategory.Drivers => "Drivers",
        DiagnosticCategory.Startup => "Startup",
        DiagnosticCategory.Services => "Services",
        DiagnosticCategory.Storage => "Storage",
        DiagnosticCategory.Network => "Network",
        _ => Result.Category.ToString()
    };
}
