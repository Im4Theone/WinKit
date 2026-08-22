using System.Windows;
using WinKit.Infrastructure;
using WinKit.Themes;
using WinKit.UI.Theming;
using WinKit.UI.ViewModels;

namespace WinKit.UI;

public partial class MainWindow : Window
{
    private readonly IThemeService _themeService;

    public MainWindow(ShellViewModel viewModel, IThemeService themeService)
    {
        Trace("MainWindow ctor begin");
        InitializeComponent();
        Trace("MainWindow InitializeComponent done");
        DataContext = viewModel;
        _themeService = themeService;

        SourceInitialized += (_, _) =>
        {
            Trace("SourceInitialized begin");
            ApplyWindowMaterial();
            Trace("SourceInitialized end");
        };
        _themeService.ThemeChanged += (_, _) => ApplyWindowMaterial();
        StateChanged += (_, _) => UpdateMaximizeGlyph();
        Trace("MainWindow ctor end");
    }

    private static void Trace(string step)
    {
        try
        {
            AppPaths.EnsureCreated();
            System.IO.File.AppendAllText(
                System.IO.Path.Combine(AppPaths.LogsDirectory, "startup-trace.log"),
                $"{DateTime.Now:O} {step}{Environment.NewLine}");
        }
        catch (Exception)
        {
            // Best-effort tracing only.
        }
    }

    private void ApplyWindowMaterial()
    {
        var theme = _themeService.Current;
        var isDark = theme.Name != "Light";
        Trace($"ApplyWindowMaterial: {theme.Material}");
        DwmWindowMaterial.Apply(this, theme.Material, isDark);
        Trace("ApplyWindowMaterial done");
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
