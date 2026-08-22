namespace WinKit.Themes;

public enum MaterialPreference
{
    Auto,
    Mica,
    Acrylic,
    Solid
}

public enum UiDensity
{
    Comfortable,
    Compact
}

public enum AnimationIntensity
{
    Full,
    Reduced,
    Off
}

public sealed class ThemeColors
{
    public string Background { get; set; } = "#101012";
    public string Surface { get; set; } = "#151517";
    public string SurfaceElevated { get; set; } = "#1A1A1D";
    public string Border { get; set; } = "#252529";
    public string TextPrimary { get; set; } = "#F5F5F7";
    public string TextSecondary { get; set; } = "#A1A1A8";
    public string TextMuted { get; set; } = "#6F6F76";
    public string Accent { get; set; } = "#4C8DFF";
    public string AccentHover { get; set; } = "#669DFF";
    public string Success { get; set; } = "#4ADE80";
    public string Warning { get; set; } = "#FBBF24";
    public string Error { get; set; } = "#F87171";
    public string Update { get; set; } = "#53B3D4";
}

/// <summary>
/// A complete, user-editable theme. Built-in themes ship read-only;
/// anything the user creates or imports is a custom theme saved to disk.
/// </summary>
public sealed class ThemeDefinition
{
    public required string Name { get; set; }
    public bool IsBuiltIn { get; set; }
    public ThemeColors Colors { get; set; } = new();
    public UiDensity Density { get; set; } = UiDensity.Comfortable;
    public double CornerRadius { get; set; } = 6;
    public double SidebarWidth { get; set; } = 232;
    public double FontScale { get; set; } = 1.0;
    public MaterialPreference Material { get; set; } = MaterialPreference.Auto;
    public AnimationIntensity AnimationIntensity { get; set; } = AnimationIntensity.Full;

    public ThemeDefinition Clone()
    {
        return new ThemeDefinition
        {
            Name = Name,
            IsBuiltIn = false,
            Colors = new ThemeColors
            {
                Background = Colors.Background,
                Surface = Colors.Surface,
                SurfaceElevated = Colors.SurfaceElevated,
                Border = Colors.Border,
                TextPrimary = Colors.TextPrimary,
                TextSecondary = Colors.TextSecondary,
                TextMuted = Colors.TextMuted,
                Accent = Colors.Accent,
                AccentHover = Colors.AccentHover,
                Success = Colors.Success,
                Warning = Colors.Warning,
                Error = Colors.Error,
                Update = Colors.Update
            },
            Density = Density,
            CornerRadius = CornerRadius,
            SidebarWidth = SidebarWidth,
            FontScale = FontScale,
            Material = Material,
            AnimationIntensity = AnimationIntensity
        };
    }
}
