namespace WinKit.Themes;

public interface IThemeService
{
    ThemeDefinition Current { get; }

    IReadOnlyList<ThemeDefinition> BuiltInThemes { get; }

    IReadOnlyList<ThemeDefinition> CustomThemes { get; }

    event EventHandler<ThemeDefinition>? ThemeChanged;

    Task InitializeAsync();

    /// <summary>Applies a theme by name. "System" resolves to Light or Midnight based on the OS app theme.</summary>
    Task ApplyThemeAsync(string name);

    /// <summary>Applies a theme transiently for live editing, without persisting it or touching the theme list.</summary>
    void Preview(ThemeDefinition theme);

    Task SaveCustomThemeAsync(ThemeDefinition theme);

    Task DeleteCustomThemeAsync(string name);

    /// <summary>Validates and imports a theme JSON file. Returns null (never throws) if the file is malformed.</summary>
    Task<ThemeDefinition?> ImportThemeAsync(string filePath);

    Task ExportThemeAsync(ThemeDefinition theme, string filePath);

    ThemeDefinition CreateDraftFrom(ThemeDefinition source, string newName);

    bool Validate(ThemeDefinition theme, out string? error);
}
