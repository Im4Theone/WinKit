using WinKit.Core.Models;

namespace WinKit.Core.Abstractions;

/// <summary>
/// Builds and submits diagnostic reports for unexpected errors. Reports are
/// built entirely from local, non-sensitive environment data and are only
/// ever sent when the user explicitly chooses "Send to Developers".
/// </summary>
public interface IErrorReportingService
{
    DiagnosticReport CreateReport(Exception exception, string component);

    Task<ErrorReportSendResult> SendReportAsync(DiagnosticReport report, CancellationToken cancellationToken = default);
}
