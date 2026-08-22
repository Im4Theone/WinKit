using System.Windows;

namespace WinKit.UI.Dialogs;

public sealed class DialogService : IDialogService
{
    public bool Confirm(ConfirmationRequest request) =>
        DialogWindow.ShowConfirmation(Application.Current.MainWindow, request);

    public void ShowError(string title, string userMessage, string? technicalDetail) =>
        DialogWindow.ShowError(Application.Current.MainWindow, title, userMessage, technicalDetail);
}
