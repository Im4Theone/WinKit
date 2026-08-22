using System.Text.Json;
using System.Text.RegularExpressions;
using Microsoft.Win32;
using WinKit.Infrastructure;

namespace WinKit.Themes;

public sealed partial class ThemeService : IThemeService
{
    public const string SystemThemeName = "System";

    private static readonly Regex HexColorPattern = HexColorRegex();

    private readonly List<ThemeDefinition> _builtIn = new();
    private readonly List<ThemeDefinition> _custom = new();

    public ThemeDefinition Current { get; private set; } = BuiltInFallback();

    public IReadOnlyList<ThemeDefinition> BuiltInThemes => _builtIn;

    public IReadOnlyList<ThemeDefinition> CustomThemes => _custom;

    public event EventHandler<ThemeDefinition>? ThemeChanged;

    public async Task InitializeAsync()
    {
        AppPaths.EnsureCreated();
        await LoadBuiltInThemesAsync();
        await LoadCustomThemesAsync();
    }

    public async Task ApplyThemeAsync(string name)
    {
        var resolvedName = name == SystemThemeName ? ResolveSystemThemeName() : name;
        var theme = FindByName(resolvedName) ?? _builtIn.First();
        Current = theme;
        ThemeChanged?.Invoke(this, theme);
        await Task.CompletedTask;
    }

    public void Preview(ThemeDefinition theme)
    {
        Current = theme;
        ThemeChanged?.Invoke(this, theme);
    }

    public async Task SaveCustomThemeAsync(ThemeDefinition theme)
    {
        if (!Validate(theme, out var error))
        {
            throw new InvalidOperationException(error ?? "Invalid theme.");
        }

        theme.IsBuiltIn = false;
        var path = CustomThemePath(theme.Name);
        await JsonFileStore.WriteAsync(path, theme, ThemeSerialization.Options);

        var existingIndex = _custom.FindIndex(t => NameEquals(t.Name, theme.Name));
        if (existingIndex >= 0)
        {
            _custom[existingIndex] = theme;
        }
        else
        {
            _custom.Add(theme);
        }

        if (NameEquals(Current.Name, theme.Name))
        {
            Current = theme;
            ThemeChanged?.Invoke(this, theme);
        }
    }

    public Task DeleteCustomThemeAsync(string name)
    {
        var path = CustomThemePath(name);
        if (File.Exists(path))
        {
            File.Delete(path);
        }

        _custom.RemoveAll(t => NameEquals(t.Name, name));
        return Task.CompletedTask;
    }

    public async Task<ThemeDefinition?> ImportThemeAsync(string filePath)
    {
        try
        {
            var json = await File.ReadAllTextAsync(filePath);
            var theme = JsonSerializer.Deserialize<ThemeDefinition>(json, ThemeSerialization.Options);
            if (theme is null || !Validate(theme, out _))
            {
                return null;
            }

            theme.IsBuiltIn = false;
            await SaveCustomThemeAsync(theme);
            return theme;
        }
        catch (JsonException)
        {
            return null;
        }
        catch (IOException)
        {
            return null;
        }
    }

    public async Task ExportThemeAsync(ThemeDefinition theme, string filePath)
    {
        var json = JsonSerializer.Serialize(theme, ThemeSerialization.Options);
        await File.WriteAllTextAsync(filePath, json);
    }

    public ThemeDefinition CreateDraftFrom(ThemeDefinition source, string newName)
    {
        var draft = source.Clone();
        draft.Name = newName;
        return draft;
    }

    public bool Validate(ThemeDefinition theme, out string? error)
    {
        if (string.IsNullOrWhiteSpace(theme.Name))
        {
            error = "Theme name cannot be empty.";
            return false;
        }

        var colors = new[]
        {
            theme.Colors.Background, theme.Colors.Surface, theme.Colors.SurfaceElevated, theme.Colors.Border,
            theme.Colors.TextPrimary, theme.Colors.TextSecondary, theme.Colors.TextMuted, theme.Colors.Accent,
            theme.Colors.AccentHover, theme.Colors.Success, theme.Colors.Warning, theme.Colors.Error, theme.Colors.Update
        };

        if (colors.Any(c => string.IsNullOrWhiteSpace(c) || !HexColorPattern.IsMatch(c)))
        {
            error = "All theme colors must be valid hex codes (e.g. #4C8DFF).";
            return false;
        }

        if (theme.CornerRadius is < 0 or > 24)
        {
            error = "Corner radius must be between 0 and 24.";
            return false;
        }

        if (theme.SidebarWidth is < 180 or > 320)
        {
            error = "Sidebar width must be between 180 and 320.";
            return false;
        }

        if (theme.FontScale is < 0.8 or > 1.4)
        {
            error = "Font scale must be between 0.8 and 1.4.";
            return false;
        }

        error = null;
        return true;
    }

    private async Task LoadBuiltInThemesAsync()
    {
        _builtIn.Clear();
        var directory = Path.Combine(AppContext.BaseDirectory, "BuiltIn");
        if (!Directory.Exists(directory))
        {
            _builtIn.Add(BuiltInFallback());
            return;
        }

        foreach (var file in Directory.EnumerateFiles(directory, "*.json").OrderBy(f => f))
        {
            var theme = await JsonFileStore.ReadAsync<ThemeDefinition>(file, ThemeSerialization.Options);
            if (theme is not null)
            {
                theme.IsBuiltIn = true;
                _builtIn.Add(theme);
            }
        }

        if (_builtIn.Count == 0)
        {
            _builtIn.Add(BuiltInFallback());
        }
    }

    private async Task LoadCustomThemesAsync()
    {
        _custom.Clear();
        if (!Directory.Exists(AppPaths.ThemesDirectory))
        {
            return;
        }

        foreach (var file in Directory.EnumerateFiles(AppPaths.ThemesDirectory, "*.json"))
        {
            var theme = await JsonFileStore.ReadAsync<ThemeDefinition>(file, ThemeSerialization.Options);
            if (theme is not null && Validate(theme, out _))
            {
                theme.IsBuiltIn = false;
                _custom.Add(theme);
            }
        }
    }

    private ThemeDefinition? FindByName(string name) =>
        _builtIn.FirstOrDefault(t => NameEquals(t.Name, name)) ??
        _custom.FirstOrDefault(t => NameEquals(t.Name, name));

    private static bool NameEquals(string a, string b) => string.Equals(a, b, StringComparison.OrdinalIgnoreCase);

    private string CustomThemePath(string name) =>
        Path.Combine(AppPaths.ThemesDirectory, $"{SanitizeFileName(name)}.json");

    private static string SanitizeFileName(string name)
    {
        var invalid = Path.GetInvalidFileNameChars();
        return new string(name.Where(c => !invalid.Contains(c)).ToArray());
    }

    private static string ResolveSystemThemeName()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(
                @"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
            var value = key?.GetValue("AppsUseLightTheme");
            var isLight = value is int i && i == 1;
            return isLight ? "Light" : "Midnight";
        }
        catch (Exception)
        {
            return "Midnight";
        }
    }

    private static ThemeDefinition BuiltInFallback() => new() { Name = "Midnight", IsBuiltIn = true };

    [GeneratedRegex("^#([0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$")]
    private static partial Regex HexColorRegex();
}
