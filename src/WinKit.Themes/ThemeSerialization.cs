using System.Text.Json;
using System.Text.Json.Serialization;

namespace WinKit.Themes;

internal static class ThemeSerialization
{
    public static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true,
        Converters = { new JsonStringEnumConverter() }
    };
}
