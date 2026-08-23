using CommunityToolkit.Mvvm.ComponentModel;
using WinKit.Themes;

namespace WinKit.UI.ViewModels;

/// <summary>
/// An editable, live-previewed copy of a ThemeDefinition. Every setter pushes
/// the change straight to IThemeService.Preview so the whole app repaints as
/// the user edits — nothing is written to disk until Save is called.
/// </summary>
public sealed class ThemeDraftViewModel : ObservableObject
{
    private readonly Action _onChanged;
    private readonly ThemeDefinition _source;

    public ThemeDraftViewModel(ThemeDefinition source, Action onChanged)
    {
        _source = source;
        _onChanged = onChanged;
        IsBuiltIn = source.IsBuiltIn;

        _name = source.Name;
        _backgroundHex = source.Colors.Background;
        _surfaceHex = source.Colors.Surface;
        _surfaceElevatedHex = source.Colors.SurfaceElevated;
        _borderHex = source.Colors.Border;
        _textPrimaryHex = source.Colors.TextPrimary;
        _textSecondaryHex = source.Colors.TextSecondary;
        _textMutedHex = source.Colors.TextMuted;
        _accentHex = source.Colors.Accent;
        _accentHoverHex = source.Colors.AccentHover;
        _onAccentHex = source.Colors.OnAccent;
        _successHex = source.Colors.Success;
        _warningHex = source.Colors.Warning;
        _errorHex = source.Colors.Error;
        _updateHex = source.Colors.Update;
        _density = source.Density;
        _cornerRadius = source.CornerRadius;
        _sidebarWidth = source.SidebarWidth;
        _fontScale = source.FontScale;
        _material = source.Material;
        _animationIntensity = source.AnimationIntensity;
    }

    public bool IsBuiltIn { get; }

    private string _name;
    public string Name { get => _name; set { if (SetProperty(ref _name, value)) Notify(); } }

    private string _backgroundHex;
    public string BackgroundHex { get => _backgroundHex; set { if (SetProperty(ref _backgroundHex, value)) Notify(); } }

    private string _surfaceHex;
    public string SurfaceHex { get => _surfaceHex; set { if (SetProperty(ref _surfaceHex, value)) Notify(); } }

    private string _surfaceElevatedHex;
    public string SurfaceElevatedHex { get => _surfaceElevatedHex; set { if (SetProperty(ref _surfaceElevatedHex, value)) Notify(); } }

    private string _borderHex;
    public string BorderHex { get => _borderHex; set { if (SetProperty(ref _borderHex, value)) Notify(); } }

    private string _textPrimaryHex;
    public string TextPrimaryHex { get => _textPrimaryHex; set { if (SetProperty(ref _textPrimaryHex, value)) Notify(); } }

    private string _textSecondaryHex;
    public string TextSecondaryHex { get => _textSecondaryHex; set { if (SetProperty(ref _textSecondaryHex, value)) Notify(); } }

    private string _textMutedHex;
    public string TextMutedHex { get => _textMutedHex; set { if (SetProperty(ref _textMutedHex, value)) Notify(); } }

    private string _accentHex;
    public string AccentHex { get => _accentHex; set { if (SetProperty(ref _accentHex, value)) Notify(); } }

    private string _accentHoverHex;
    public string AccentHoverHex { get => _accentHoverHex; set { if (SetProperty(ref _accentHoverHex, value)) Notify(); } }

    private string _onAccentHex;
    public string OnAccentHex { get => _onAccentHex; set { if (SetProperty(ref _onAccentHex, value)) Notify(); } }

    private string _successHex;
    public string SuccessHex { get => _successHex; set { if (SetProperty(ref _successHex, value)) Notify(); } }

    private string _warningHex;
    public string WarningHex { get => _warningHex; set { if (SetProperty(ref _warningHex, value)) Notify(); } }

    private string _errorHex;
    public string ErrorHex { get => _errorHex; set { if (SetProperty(ref _errorHex, value)) Notify(); } }

    private string _updateHex;
    public string UpdateHex { get => _updateHex; set { if (SetProperty(ref _updateHex, value)) Notify(); } }

    private UiDensity _density;
    public UiDensity Density { get => _density; set { if (SetProperty(ref _density, value)) Notify(); } }

    private double _cornerRadius;
    public double CornerRadius { get => _cornerRadius; set { if (SetProperty(ref _cornerRadius, value)) Notify(); } }

    private double _sidebarWidth;
    public double SidebarWidth { get => _sidebarWidth; set { if (SetProperty(ref _sidebarWidth, value)) Notify(); } }

    private double _fontScale;
    public double FontScale { get => _fontScale; set { if (SetProperty(ref _fontScale, value)) Notify(); } }

    private MaterialPreference _material;
    public MaterialPreference Material { get => _material; set { if (SetProperty(ref _material, value)) Notify(); } }

    private AnimationIntensity _animationIntensity;
    public AnimationIntensity AnimationIntensity { get => _animationIntensity; set { if (SetProperty(ref _animationIntensity, value)) Notify(); } }

    public IReadOnlyList<UiDensity> DensityOptions { get; } = Enum.GetValues<UiDensity>();
    public IReadOnlyList<MaterialPreference> MaterialOptions { get; } = Enum.GetValues<MaterialPreference>();
    public IReadOnlyList<AnimationIntensity> AnimationOptions { get; } = Enum.GetValues<AnimationIntensity>();

    public ThemeDefinition ToDefinition() => new()
    {
        Name = Name,
        IsBuiltIn = false,
        Colors = new ThemeColors
        {
            Background = BackgroundHex,
            Surface = SurfaceHex,
            SurfaceElevated = SurfaceElevatedHex,
            Border = BorderHex,
            TextPrimary = TextPrimaryHex,
            TextSecondary = TextSecondaryHex,
            TextMuted = TextMutedHex,
            Accent = AccentHex,
            AccentHover = AccentHoverHex,
            OnAccent = OnAccentHex,
            Success = SuccessHex,
            Warning = WarningHex,
            Error = ErrorHex,
            Update = UpdateHex
        },
        Density = Density,
        CornerRadius = CornerRadius,
        SidebarWidth = SidebarWidth,
        FontScale = FontScale,
        Material = Material,
        AnimationIntensity = AnimationIntensity
    };

    private static readonly System.Text.RegularExpressions.Regex HexPattern =
        new("^#([0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$");

    private void Notify()
    {
        // Only push a live preview once every color field is a well-formed hex value;
        // this keeps mid-typing states from flashing an invalid theme onto the whole app.
        string[] colors = { BackgroundHex, SurfaceHex, SurfaceElevatedHex, BorderHex, TextPrimaryHex,
            TextSecondaryHex, TextMutedHex, AccentHex, AccentHoverHex, OnAccentHex, SuccessHex, WarningHex, ErrorHex, UpdateHex };

        if (colors.All(c => HexPattern.IsMatch(c)))
        {
            _onChanged();
        }
    }

    public ThemeDefinition Source => _source;
}
