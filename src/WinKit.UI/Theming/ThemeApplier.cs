using System.Windows;
using System.Windows.Media;
using WinKit.Themes;

namespace WinKit.UI.Theming;

/// <summary>
/// Bridges a WinKit.Themes.ThemeDefinition onto live WPF resources. Every
/// value it writes goes straight into Application.Resources' own entries,
/// which DynamicResource consumers resolve ahead of the merged-dictionary
/// defaults in Resources/Brushes.xaml etc. — so the whole UI repaints the
/// moment a theme changes, with no restart and no per-control wiring.
/// </summary>
public sealed class ThemeApplier
{
    private readonly IThemeService _themeService;

    public ThemeApplier(IThemeService themeService)
    {
        _themeService = themeService;
        _themeService.ThemeChanged += (_, theme) => Apply(theme);
    }

    public void ApplyCurrent() => Apply(_themeService.Current);

    private static void Apply(ThemeDefinition theme)
    {
        var resources = Application.Current.Resources;
        var colors = theme.Colors;

        SetColor(resources, "Color.Background", colors.Background);
        SetColor(resources, "Color.Surface", colors.Surface);
        SetColor(resources, "Color.SurfaceElevated", colors.SurfaceElevated);
        SetColor(resources, "Color.Border", colors.Border);
        SetColor(resources, "Color.TextPrimary", colors.TextPrimary);
        SetColor(resources, "Color.TextSecondary", colors.TextSecondary);
        SetColor(resources, "Color.TextMuted", colors.TextMuted);
        SetColor(resources, "Color.Accent", colors.Accent);
        SetColor(resources, "Color.AccentHover", colors.AccentHover);
        SetColor(resources, "Color.OnAccent", colors.OnAccent);
        SetColor(resources, "Color.Success", colors.Success);
        SetColor(resources, "Color.Warning", colors.Warning);
        SetColor(resources, "Color.Error", colors.Error);
        SetColor(resources, "Color.Update", colors.Update);

        resources["Theme.CornerRadius"] = new CornerRadius(theme.CornerRadius);

        // Compact controls (buttons, text inputs) use this instead of the raw corner
        // radius. Capped rather than merely smaller: at extreme settings (e.g. 24) the
        // full radius on a ~30px-tall control rounds past its own content, clipping
        // button text — capping keeps every setting usable while staying identical to
        // the uncapped value at normal (low-to-moderate) corner radii.
        resources["Theme.CornerRadiusSmall"] = new CornerRadius(Math.Min(theme.CornerRadius, 12));
        resources["Theme.SidebarWidth"] = new GridLength(theme.SidebarWidth);

        var densityScale = theme.Density == UiDensity.Compact ? 0.85 : 1.0;
        var fontScale = theme.FontScale * densityScale;
        resources["Font.Size.Small"] = 11.5 * fontScale;
        resources["Font.Size.Body"] = 13.0 * fontScale;
        resources["Font.Size.Title"] = 15.0 * fontScale;
        resources["Font.Size.Heading"] = 21.0 * fontScale;
        resources["Font.Size.Display"] = 28.0 * fontScale;

        var (fast, normal) = theme.AnimationIntensity switch
        {
            AnimationIntensity.Off => (TimeSpan.Zero, TimeSpan.Zero),
            AnimationIntensity.Reduced => (TimeSpan.FromSeconds(0.06), TimeSpan.FromSeconds(0.08)),
            _ => (TimeSpan.FromSeconds(0.12), TimeSpan.FromSeconds(0.16))
        };
        resources["Anim.Duration.Fast"] = new Duration(fast);
        resources["Anim.Duration.Normal"] = new Duration(normal);

        AnimationProfile.CurrentIntensity = theme.AnimationIntensity;
    }

    private static void SetColor(ResourceDictionary resources, string key, string hex)
    {
        if (ColorConverter.ConvertFromString(hex) is Color color)
        {
            resources[key] = color;
        }
    }
}

public static class AnimationProfile
{
    public static AnimationIntensity CurrentIntensity { get; set; } = AnimationIntensity.Full;
}
