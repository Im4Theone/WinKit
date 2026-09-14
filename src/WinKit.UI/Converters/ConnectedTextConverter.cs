using System.Globalization;
using System.Windows.Data;

namespace WinKit.UI.Converters;

public sealed class ConnectedTextConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        value is true ? "Connected" : "Not connected";

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
