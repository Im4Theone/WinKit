using System.Globalization;
using System.Windows.Data;

namespace WinKit.UI.Converters;

public sealed class BytesToReadableConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        var bytes = value switch
        {
            long l => l,
            int i => i,
            double d => d,
            _ => 0d
        };

        string[] units = { "B", "KB", "MB", "GB", "TB" };
        double size = System.Convert.ToDouble(bytes);
        var unitIndex = 0;
        while (size >= 1024 && unitIndex < units.Length - 1)
        {
            size /= 1024;
            unitIndex++;
        }

        return $"{size:0.#} {units[unitIndex]}";
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
