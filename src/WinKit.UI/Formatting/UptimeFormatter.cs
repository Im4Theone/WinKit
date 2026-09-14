namespace WinKit.UI.Formatting;

public static class UptimeFormatter
{
    public static string Format(TimeSpan uptime) =>
        uptime.TotalDays >= 1
            ? $"{(int)uptime.TotalDays}d {uptime.Hours}h {uptime.Minutes}m {uptime.Seconds}s"
            : uptime.Hours > 0
                ? $"{uptime.Hours}h {uptime.Minutes}m {uptime.Seconds}s"
                : $"{uptime.Minutes}m {uptime.Seconds}s";
}
