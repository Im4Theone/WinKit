using System.Collections;
using System.Globalization;
using System.Windows.Data;

namespace WinKit.UI.Converters;

public sealed class FirstOrPlaceholderConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is IEnumerable enumerable and not string)
        {
            foreach (var item in enumerable)
            {
                return item?.ToString() ?? "-";
            }
        }

        return "-";
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
