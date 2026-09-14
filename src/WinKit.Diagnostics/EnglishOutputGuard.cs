using System.Globalization;

namespace WinKit.Diagnostics;

/// <summary>
/// DISM and SFC localize their console output to the OS's installed UI language,
/// with no reliable machine-readable alternative (exit codes alone can't
/// distinguish "clean" from "repaired" from "unrepairable"). Detection/repair
/// logic that greps for specific English phrases must check this first and
/// report NotApplicable/fail closed rather than silently misreading a
/// different language's output as a false positive or false failure.
/// </summary>
internal static class EnglishOutputGuard
{
    public static string CurrentUiCultureName => CultureInfo.InstalledUICulture.Name;

    public static bool IsExpected =>
        CultureInfo.InstalledUICulture.TwoLetterISOLanguageName.Equals("en", StringComparison.OrdinalIgnoreCase);
}
