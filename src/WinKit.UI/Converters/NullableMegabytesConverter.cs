using System.Globalization;
using System.Windows.Data;

namespace WinKit.UI.Converters;

public sealed class NullableMegabytesConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is not double sizeMb)
        {
            return string.Empty;
        }

        return sizeMb >= 1024
            ? $"{sizeMb / 1024:0.#} GB"
            : $"{sizeMb:0} MB";
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
