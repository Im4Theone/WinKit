using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;
using WinKit.Themes;

namespace WinKit.UI.Theming;

/// <summary>
/// Applies Windows 11 Mica composition to a window via DWM. Silently no-ops
/// on older Windows builds or if composition is unavailable, leaving the
/// window's ordinary solid background in place.
/// </summary>
internal static class DwmWindowMaterial
{
    private const int DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
    private const int DWMWA_WINDOW_CORNER_PREFERENCE = 33;
    private const int DWMWA_SYSTEMBACKDROP_TYPE = 38;
    private const int DWMSBT_NONE = 1;
    private const int DWMSBT_MAINWINDOW = 2; // Mica
    private const int DWMSBT_TRANSIENTWINDOW = 3; // Acrylic-like
    private const int DWMWCP_ROUND = 2;

    [DllImport("dwmapi.dll", PreserveSig = true)]
    private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);

    [DllImport("dwmapi.dll", PreserveSig = true)]
    private static extern int DwmExtendFrameIntoClientArea(IntPtr hwnd, ref Margins margins);

    [StructLayout(LayoutKind.Sequential)]
    private struct Margins
    {
        public int Left, Right, Top, Bottom;
    }

    public static bool IsSupported => Environment.OSVersion.Version.Build >= 22000;

    public static bool Apply(Window window, MaterialPreference preference, bool isDarkTheme)
    {
        if (!IsSupported)
        {
            return false;
        }

        var hwndSource = HwndSource.FromVisual(window) as HwndSource;
        if (hwndSource is null)
        {
            return false;
        }

        var hwnd = hwndSource.Handle;

        var darkMode = isDarkTheme ? 1 : 0;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, ref darkMode, sizeof(int));

        var corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, ref corner, sizeof(int));

        var wantsMaterial = preference is MaterialPreference.Auto or MaterialPreference.Mica or MaterialPreference.Acrylic;
        if (!wantsMaterial || Environment.OSVersion.Version.Build < 22621)
        {
            var none = DWMSBT_NONE;
            DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, ref none, sizeof(int));
            return false;
        }

        var backdrop = preference == MaterialPreference.Acrylic ? DWMSBT_TRANSIENTWINDOW : DWMSBT_MAINWINDOW;
        var hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, ref backdrop, sizeof(int));
        if (hr != 0)
        {
            return false;
        }

        var margins = new Margins { Left = -1, Right = -1, Top = -1, Bottom = -1 };
        DwmExtendFrameIntoClientArea(hwnd, ref margins);

        hwndSource.CompositionTarget.BackgroundColor = Colors.Transparent;
        window.Background = Brushes.Transparent;
        return true;
    }
}
