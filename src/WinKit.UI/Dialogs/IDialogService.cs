namespace WinKit.UI.Dialogs;

public interface IDialogService
{
    /// <summary>Shows a modal confirmation dialog and returns true if the user confirmed.</summary>
    bool Confirm(ConfirmationRequest request);

    /// <summary>Shows a friendly error dialog with an expandable technical-detail section.</summary>
    void ShowError(string title, string userMessage, string? technicalDetail);
}
