using System.Runtime.InteropServices;

namespace WinKit.Cleanup;

[StructLayout(LayoutKind.Sequential)]
internal struct ShQueryRbInfo
{
    public int cbSize;
    public long i64Size;
    public long i64NumItems;
}

internal static class RecycleBinInterop
{
    private const int SHERB_NOCONFIRMATION = 0x00000001;
    private const int SHERB_NOPROGRESSUI = 0x00000002;
    private const int SHERB_NOSOUND = 0x00000004;

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern int SHQueryRecycleBin(string? pszRootPath, ref ShQueryRbInfo pSHQueryRBInfo);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern int SHEmptyRecycleBin(IntPtr hwnd, string? pszRootPath, int dwFlags);

    public static (long sizeBytes, long itemCount) Query()
    {
        var info = new ShQueryRbInfo { cbSize = Marshal.SizeOf<ShQueryRbInfo>() };
        var hr = SHQueryRecycleBin(null, ref info);
        return hr == 0 ? (info.i64Size, info.i64NumItems) : (0, 0);
    }

    public static bool Empty()
    {
        var hr = SHEmptyRecycleBin(IntPtr.Zero, null, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        // 0 = success, -2147418113 (E_UNEXPECTED) is also returned when the bin is already empty.
        return hr == 0 || unchecked((uint)hr) == 0x8000FFFF;
    }
}
