using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace WinKit.UI.Theming;

/// <summary>
/// Applies the window-level DWM attributes WinKit still controls directly:
/// immersive dark mode (for a dark title bar/frame) and rounded corners.
/// Silently no-ops on older Windows builds or if composition is unavailable.
/// </summary>
internal static class DwmWindowMaterial
{
    private const int DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
    private const int DWMWA_WINDOW_CORNER_PREFERENCE = 33;
    private const int DWMWCP_ROUND = 2;

    [DllImport("dwmapi.dll", PreserveSig = true)]
    private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);

    public static bool IsSupported => Environment.OSVersion.Version.Build >= 22000;

    public static void Apply(Window window, bool isDarkTheme)
    {
        if (!IsSupported)
        {
            return;
        }

        var hwndSource = HwndSource.FromVisual(window) as HwndSource;
        if (hwndSource is null)
        {
            return;
        }

        var hwnd = hwndSource.Handle;

        var darkMode = isDarkTheme ? 1 : 0;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, ref darkMode, sizeof(int));

        var corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, ref corner, sizeof(int));
    }
}
