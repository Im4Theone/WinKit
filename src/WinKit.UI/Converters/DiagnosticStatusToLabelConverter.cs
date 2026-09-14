using System.Globalization;
using System.Windows.Data;
using WinKit.Core.Models;

namespace WinKit.UI.Converters;

public sealed class DiagnosticStatusToLabelConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture) => value switch
    {
        DiagnosticStatus.Passed => "PASSED",
        DiagnosticStatus.Warning => "WARNING",
        DiagnosticStatus.Failed => "FAILED",
        DiagnosticStatus.Informational => "INFO",
        DiagnosticStatus.NotApplicable => "N/A",
        _ => string.Empty
    };

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
