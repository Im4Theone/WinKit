using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;

namespace WinKit.UI.Converters;

public sealed class ResourceKeyToGeometryConverter : IValueConverter
{
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is string key && Application.Current.TryFindResource(key) is Geometry geometry)
        {
            return geometry;
        }

        return null;
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
