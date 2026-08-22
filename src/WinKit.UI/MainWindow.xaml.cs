using System.Windows;
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
        var isDark = theme.Name != "Light";
        DwmWindowMaterial.Apply(this, theme.Material, isDark);
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
