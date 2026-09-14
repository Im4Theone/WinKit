using System.Net.Http.Json;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.Win32;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;

namespace WinKit.Infrastructure;

public sealed class ErrorReportingService : IErrorReportingService
{
    // Deployed Cloudflare Worker (see backend/error-report-worker). Report sending fails
    // gracefully (caught, surfaced to the user, retryable) if this is ever unreachable.
    private const string ReportEndpoint = "https://winkit-error-reports.kaloyankrastev2013.workers.dev/report";

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase
    };

    private readonly HttpClient _httpClient;

    public ErrorReportingService(HttpClient httpClient)
    {
        _httpClient = httpClient;
    }

    public DiagnosticReport CreateReport(Exception exception, string component)
    {
        return new DiagnosticReport
        {
            ErrorId = GenerateErrorId(),
            WinKitVersion = GetWinKitVersion(),
            WindowsVersion = GetWindowsVersion(),
            Architecture = RuntimeInformation.ProcessArchitecture.ToString(),
            Component = component,
            ExceptionType = exception.GetType().FullName ?? exception.GetType().Name,
            ExceptionMessage = exception.Message,
            StackTrace = exception.StackTrace ?? "(no stack trace available)",
            Timestamp = DateTimeOffset.UtcNow
        };
    }

    public async Task<ErrorReportSendResult> SendReportAsync(DiagnosticReport report, CancellationToken cancellationToken = default)
    {
        try
        {
            var payload = new
            {
                errorId = report.ErrorId,
                winKitVersion = report.WinKitVersion,
                windowsVersion = report.WindowsVersion,
                architecture = report.Architecture,
                component = report.Component,
                exceptionType = report.ExceptionType,
                exceptionMessage = report.ExceptionMessage,
                stackTrace = report.StackTrace,
                timestamp = report.Timestamp
            };

            using var response = await _httpClient.PostAsJsonAsync(ReportEndpoint, payload, JsonOptions, cancellationToken);
            if (!response.IsSuccessStatusCode)
            {
                return ErrorReportSendResult.Failed($"The server responded with {(int)response.StatusCode} {response.ReasonPhrase}.");
            }

            return ErrorReportSendResult.Ok();
        }
        catch (HttpRequestException ex)
        {
            return ErrorReportSendResult.Failed($"Couldn't reach the reporting server ({ex.Message}).");
        }
        catch (TaskCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            return ErrorReportSendResult.Failed("The request timed out.");
        }
        catch (JsonException)
        {
            return ErrorReportSendResult.Failed("The server returned an unexpected response.");
        }
    }

    private static string GenerateErrorId() => "WK-" + Convert.ToHexString(RandomNumberGenerator.GetBytes(4));

    private static string GetWinKitVersion() =>
        Assembly.GetEntryAssembly()?.GetName().Version?.ToString(3) ?? "unknown";

    private static string GetWindowsVersion()
    {
        try
        {
            using var key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Microsoft\Windows NT\CurrentVersion");
            var productName = key?.GetValue("ProductName") as string ?? "Windows";
            var displayVersion = key?.GetValue("DisplayVersion") as string;
            var buildNumber = key?.GetValue("CurrentBuildNumber") as string;

            if (int.TryParse(buildNumber, out var build) && build >= 22000)
            {
                productName = productName.Replace("Windows 10", "Windows 11", StringComparison.Ordinal);
            }

            return string.IsNullOrEmpty(displayVersion)
                ? $"{productName} (Build {buildNumber})"
                : $"{productName} {displayVersion} (Build {buildNumber})";
        }
        catch (Exception ex) when (ex is System.Security.SecurityException or ObjectDisposedException)
        {
            return Environment.OSVersion.VersionString;
        }
    }
}
