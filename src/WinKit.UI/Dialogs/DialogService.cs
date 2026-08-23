using System.Windows;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;

namespace WinKit.UI.Dialogs;

public sealed class DialogService : IDialogService
{
    private readonly IErrorReportingService _errorReportingService;

    public DialogService(IErrorReportingService errorReportingService)
    {
        _errorReportingService = errorReportingService;
    }

    public bool Confirm(ConfirmationRequest request) =>
        DialogWindow.ShowConfirmation(Application.Current.MainWindow, request);

    public void ShowError(string title, string userMessage, string? technicalDetail) =>
        DialogWindow.ShowError(Application.Current.MainWindow, title, userMessage, technicalDetail);

    public void ShowErrorReport(string title, string userMessage, DiagnosticReport report) =>
        DialogWindow.ShowErrorReport(Application.Current.MainWindow, title, userMessage, report, _errorReportingService.SendReportAsync);
}
