using System.Text.RegularExpressions;
using WinKit.Core.Models;
using WinKit.Infrastructure;
using Xunit;

namespace WinKit.Infrastructure.Tests;

public sealed class ErrorReportingServiceTests
{
    // Locks the DiagnosticReport schema to exactly what item 3 of the spec allows.
    // If this fails after adding a field, that field needs to be deliberately
    // reviewed for sensitivity (user files, credentials, activity history, full
    // settings, theme contents) before it's allowed into a report.
    private static readonly HashSet<string> AllowedFields = new(StringComparer.Ordinal)
    {
        nameof(DiagnosticReport.ErrorId),
        nameof(DiagnosticReport.WinKitVersion),
        nameof(DiagnosticReport.WindowsVersion),
        nameof(DiagnosticReport.Architecture),
        nameof(DiagnosticReport.Component),
        nameof(DiagnosticReport.ExceptionType),
        nameof(DiagnosticReport.ExceptionMessage),
        nameof(DiagnosticReport.StackTrace),
        nameof(DiagnosticReport.Timestamp)
    };

    [Fact]
    public void DiagnosticReport_ExposesOnlyAllowedFields()
    {
        var actualFields = typeof(DiagnosticReport)
            .GetProperties()
            .Select(p => p.Name)
            .ToHashSet(StringComparer.Ordinal);

        Assert.Equal(AllowedFields, actualFields);
    }

    [Fact]
    public void CreateReport_MapsExceptionDetailsAndGeneratesValidErrorId()
    {
        var service = new ErrorReportingService(new HttpClient());
        Exception exception;
        try
        {
            throw new InvalidOperationException("boom");
        }
        catch (Exception ex)
        {
            exception = ex;
        }

        var report = service.CreateReport(exception, "TestComponent");

        Assert.Matches(new Regex("^WK-[0-9A-F]{8}$"), report.ErrorId);
        Assert.Equal("TestComponent", report.Component);
        Assert.Equal(typeof(InvalidOperationException).FullName, report.ExceptionType);
        Assert.Equal("boom", report.ExceptionMessage);
        Assert.Contains("CreateReport_MapsExceptionDetailsAndGeneratesValidErrorId", report.StackTrace);
        Assert.NotEmpty(report.WinKitVersion);
        Assert.NotEmpty(report.WindowsVersion);
        Assert.NotEmpty(report.Architecture);
    }

    [Fact]
    public void CreateReport_GeneratesUniqueErrorIdsAcrossCalls()
    {
        var service = new ErrorReportingService(new HttpClient());
        var exception = new Exception("test");

        var ids = Enumerable.Range(0, 20)
            .Select(_ => service.CreateReport(exception, "Test").ErrorId)
            .ToHashSet();

        Assert.Equal(20, ids.Count);
    }

    [Fact]
    public void ToReportText_IncludesAllFieldsInReadableForm()
    {
        var report = new DiagnosticReport
        {
            ErrorId = "WK-DEADBEEF",
            WinKitVersion = "1.0.1",
            WindowsVersion = "Windows 11 Pro (Build 26200)",
            Architecture = "X64",
            Component = "TestComponent",
            ExceptionType = "System.Exception",
            ExceptionMessage = "Something broke",
            StackTrace = "   at Test.Method()",
            Timestamp = DateTimeOffset.UtcNow
        };

        var text = report.ToReportText();

        Assert.Contains("WK-DEADBEEF", text);
        Assert.Contains("1.0.1", text);
        Assert.Contains("Windows 11 Pro (Build 26200)", text);
        Assert.Contains("TestComponent", text);
        Assert.Contains("Something broke", text);
        Assert.Contains("at Test.Method()", text);
    }
}
