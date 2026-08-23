using System.Windows;
using System.Windows.Media;
using WinKit.Themes;
using WinKit.UI.Theming;
using WinKit.UI.ViewModels;

namespace WinKit.UI;

public partial class MainWindow : Window
{
    private readonly IThemeService _themeService;

    public MainWindow(ShellViewModel viewModel, IThemeService themeService)
    {
        InitializeComponent();
        DataContext = viewModel;
        _themeService = themeService;

        SourceInitialized += (_, _) => ApplyWindowMaterial();
        _themeService.ThemeChanged += (_, _) => ApplyWindowMaterial();
        StateChanged += (_, _) => UpdateMaximizeGlyph();
    }

    private void ApplyWindowMaterial()
    {
        var theme = _themeService.Current;
        var isDark = IsDarkBackground(theme.Colors.Background);
        var materialApplied = DwmWindowMaterial.Apply(this, theme.Material, isDark);

        // DwmWindowMaterial makes the native composition surface transparent so
        // Mica/Acrylic can show through; RootGrid's own opaque background would
        // otherwise paint straight over it. Clearing it reverts to the normal
        // DynamicResource-bound background when no material is actually active
        // (older Windows builds, MaterialPreference.Solid, or a failed apply).
        if (materialApplied)
        {
            RootGrid.Background = Brushes.Transparent;
        }
        else
        {
            RootGrid.ClearValue(BackgroundProperty);
        }
    }

    private static bool IsDarkBackground(string backgroundHex)
    {
        if (ColorConverter.ConvertFromString(backgroundHex) is not Color color)
        {
            return true;
        }

        var luminance = (0.299 * color.R + 0.587 * color.G + 0.114 * color.B) / 255.0;
        return luminance < 0.5;
    }

    private void OnMinimizeClick(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;

    private void OnMaximizeRestoreClick(object sender, RoutedEventArgs e)
    {
        WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;
    }

    private void OnCloseClick(object sender, RoutedEventArgs e) => Close();

    private void UpdateMaximizeGlyph()
    {
        var key = WindowState == WindowState.Maximized ? "Icon.Restore" : "Icon.Maximize";
        MaximizeIcon.Data = (System.Windows.Media.Geometry)FindResource(key);
    }
}
