using System.Windows;
using System.Windows.Media;
using WinKit.Core.Models;

namespace WinKit.UI.Dialogs;

public partial class DialogWindow : Window
{
    private DiagnosticReport? _report;
    private Func<DiagnosticReport, CancellationToken, Task<ErrorReportSendResult>>? _sendAsync;

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

    /// <summary>Shows an update confirmation with the release's changelog visible, and Cancel / Install Update actions.</summary>
    public static bool ShowUpdateConfirmation(Window? owner, string version, string? releaseNotes)
    {
        var dialog = new DialogWindow { Owner = owner };
        dialog.TitleText.Text = $"WinKit {version} is available";
        dialog.MessageText.Text = "Review what's changed, then choose whether to install it now. Your settings, themes, and activity history are preserved.";
        dialog.ConfirmButton.Content = "Install Update";
        dialog.CancelButton.Content = "Cancel";

        if (!string.IsNullOrWhiteSpace(releaseNotes))
        {
            dialog.DetailsExpander.Header = "What's new";
            dialog.DetailsExpander.IsExpanded = true;
            dialog.DetailsExpander.Visibility = Visibility.Visible;
            dialog.DetailsText.Text = releaseNotes;
            dialog.CopyButton.Content = "Copy changelog";
        }

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

    /// <summary>
    /// Shows the reusable "Preview / Close / Send to Developers" error-report dialog.
    /// <paramref name="sendAsync"/> is invoked only if the user explicitly clicks
    /// "Send to Developers" — nothing is transmitted otherwise.
    /// </summary>
    public static void ShowErrorReport(
        Window? owner,
        string title,
        string userMessage,
        DiagnosticReport report,
        Func<DiagnosticReport, CancellationToken, Task<ErrorReportSendResult>> sendAsync)
    {
        var dialog = new DialogWindow { Owner = owner };
        dialog.TitleText.Text = title;
        dialog.MessageText.Text = userMessage;
        dialog._report = report;
        dialog._sendAsync = sendAsync;

        dialog.ErrorIdText.Text = $"Error ID: {report.ErrorId}";
        dialog.ErrorIdText.Visibility = Visibility.Visible;

        dialog.DetailsExpander.Header = "Preview report";
        dialog.DetailsText.Text = report.ToReportText();
        dialog.DetailsExpander.Visibility = Visibility.Visible;
        dialog.CopyButton.Content = "Copy report";

        dialog.PreviewButton.Visibility = Visibility.Visible;
        dialog.CancelButton.Content = "Close";
        dialog.ConfirmButton.Content = "Send to Developers";

        dialog.ShowDialog();
    }

    private void OnPreviewClick(object sender, RoutedEventArgs e)
    {
        DetailsExpander.IsExpanded = !DetailsExpander.IsExpanded;
    }

    private async void OnConfirmClick(object sender, RoutedEventArgs e)
    {
        if (_sendAsync is not null && _report is not null)
        {
            await SendReportAsync();
            return;
        }

        DialogResult = true;
    }

    private async Task SendReportAsync()
    {
        ConfirmButton.IsEnabled = false;
        PreviewButton.IsEnabled = false;
        StatusText.Foreground = (Brush)FindResource("Brush.TextSecondary");
        StatusText.Text = "Sending...";
        StatusText.Visibility = Visibility.Visible;

        var result = await _sendAsync!(_report!, CancellationToken.None);

        if (result.Status == ErrorReportSendStatus.Success)
        {
            StatusText.Foreground = (Brush)FindResource("Brush.Success");
            StatusText.Text = $"Error report sent successfully. Report ID: {_report!.ErrorId}";
            PreviewButton.Visibility = Visibility.Collapsed;
            ConfirmButton.Visibility = Visibility.Collapsed;
            CancelButton.Content = "Close";
        }
        else
        {
            StatusText.Foreground = (Brush)FindResource("Brush.Error");
            StatusText.Text = $"Couldn't send the report: {result.ErrorMessage} The report hasn't been lost — you can retry or copy it.";
            ConfirmButton.Content = "Retry";
            ConfirmButton.IsEnabled = true;
            PreviewButton.IsEnabled = true;
        }
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
