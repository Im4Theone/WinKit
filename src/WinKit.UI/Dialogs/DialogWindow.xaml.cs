using System.Windows;

namespace WinKit.UI.Dialogs;

public partial class DialogWindow : Window
{
    public DialogWindow()
    {
        InitializeComponent();
    }

    public static bool ShowConfirmation(Window? owner, ConfirmationRequest request)
    {
        var dialog = new DialogWindow { Owner = owner };
        dialog.TitleText.Text = request.Title;
        dialog.MessageText.Text = request.Message;
        dialog.ConfirmButton.Content = request.ConfirmText;
        dialog.CancelButton.Content = request.CancelText;
        dialog.ConfirmButton.Style = (Style)dialog.FindResource(
            request.IsDestructive ? "Button.Destructive" : "Button.Primary");

        return dialog.ShowDialog() == true;
    }

    public static void ShowError(Window? owner, string title, string userMessage, string? technicalDetail)
    {
        var dialog = new DialogWindow { Owner = owner };
        dialog.TitleText.Text = title;
        dialog.MessageText.Text = userMessage;
        dialog.CancelButton.Visibility = Visibility.Collapsed;
        dialog.ConfirmButton.Content = "OK";

        if (!string.IsNullOrWhiteSpace(technicalDetail))
        {
            dialog.DetailsExpander.Visibility = Visibility.Visible;
            dialog.DetailsText.Text = technicalDetail;
        }

        dialog.ShowDialog();
    }

    private void OnConfirmClick(object sender, RoutedEventArgs e)
    {
        DialogResult = true;
    }

    private void OnCancelClick(object sender, RoutedEventArgs e)
    {
        DialogResult = false;
    }

    private void OnCopyDetailsClick(object sender, RoutedEventArgs e)
    {
        try
        {
            Clipboard.SetText(DetailsText.Text);
        }
        catch (System.Runtime.InteropServices.ExternalException)
        {
            // Clipboard can be transiently locked by another process; not critical here.
        }
    }
}
