using WinKit.Core.Models;

namespace WinKit.UI.Dialogs;

public interface IDialogService
{
    /// <summary>Shows a modal confirmation dialog and returns true if the user confirmed.</summary>
    bool Confirm(ConfirmationRequest request);

    /// <summary>Shows a friendly error dialog with an expandable technical-detail section.</summary>
    void ShowError(string title, string userMessage, string? technicalDetail);

    /// <summary>Shows the reusable error-report dialog (Preview / Close / Send to Developers).</summary>
    void ShowErrorReport(string title, string userMessage, DiagnosticReport report);

    /// <summary>Shows the update changelog with Cancel / Install Update actions; returns true if the user chose to install.</summary>
    bool ShowUpdateConfirmation(string version, string? releaseNotes);
}
