using System.Text;

namespace WinKit.Core.Models;

/// <summary>
/// A diagnostic snapshot for one unexpected error, built for the user to review
/// (via "Preview") before anything is sent. Deliberately excludes anything that
/// isn't needed to reproduce/diagnose the bug: no user files, credentials,
/// activity history, full settings, or theme contents.
/// </summary>
public sealed class DiagnosticReport
{
    public required string ErrorId { get; init; }
    public required string WinKitVersion { get; init; }
    public required string WindowsVersion { get; init; }
    public required string Architecture { get; init; }
    public required string Component { get; init; }
    public required string ExceptionType { get; init; }
    public required string ExceptionMessage { get; init; }
    public required string StackTrace { get; init; }
    public required DateTimeOffset Timestamp { get; init; }

    /// <summary>The exact text shown on the Preview screen and submitted as the report body.</summary>
    public string ToReportText()
    {
        var sb = new StringBuilder();
        sb.AppendLine($"Error ID: {ErrorId}");
        sb.AppendLine($"Component: {Component}");
        sb.AppendLine($"Timestamp: {Timestamp:O}");
        sb.AppendLine();
        sb.AppendLine($"WinKit Version: {WinKitVersion}");
        sb.AppendLine($"Windows Version: {WindowsVersion}");
        sb.AppendLine($"Architecture: {Architecture}");
        sb.AppendLine();
        sb.AppendLine($"Exception Type: {ExceptionType}");
        sb.AppendLine($"Exception Message: {ExceptionMessage}");
        sb.AppendLine();
        sb.AppendLine("Stack Trace:");
        sb.AppendLine(StackTrace);
        return sb.ToString();
    }
}

public enum ErrorReportSendStatus
{
    Success,
    Failed
}

public sealed class ErrorReportSendResult
{
    public required ErrorReportSendStatus Status { get; init; }
    public string? ErrorMessage { get; init; }

    public static ErrorReportSendResult Ok() => new() { Status = ErrorReportSendStatus.Success };

    public static ErrorReportSendResult Failed(string errorMessage) =>
        new() { Status = ErrorReportSendStatus.Failed, ErrorMessage = errorMessage };
}
