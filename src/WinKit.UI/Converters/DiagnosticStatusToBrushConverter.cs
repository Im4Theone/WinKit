using System.Globalization;
using System.Windows;
using System.Windows.Data;
using WinKit.Core.Models;

namespace WinKit.UI.Converters;

public sealed class DiagnosticStatusToBrushConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        var key = value switch
        {
            DiagnosticStatus.Passed => "Brush.Success",
            DiagnosticStatus.Warning => "Brush.Warning",
            DiagnosticStatus.Failed => "Brush.Error",
            DiagnosticStatus.Informational => "Brush.Accent",
            DiagnosticStatus.NotApplicable => "Brush.TextMuted",
            _ => "Brush.TextMuted"
        };

        return Application.Current.TryFindResource(key) ?? System.Windows.Media.Brushes.Gray;
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
