namespace WinKit.Cleanup.Models;

public enum CleanupCategory
{
    UserTemp,
    WindowsTemp,
    ThumbnailCache,
    RecycleBin,
    WindowsUpdateCache
}

public sealed class CleanupCategoryScan
{
    public required CleanupCategory Category { get; init; }
    public required string DisplayName { get; init; }
    public required string Description { get; init; }
    public required IReadOnlyList<string> Locations { get; init; }
    public long SizeBytes { get; init; }
    public int FileCount { get; init; }
    public double SizeGb => SizeBytes / 1024.0 / 1024.0 / 1024.0;
}

public sealed class CleanupSummary
{
    public long BytesFreed { get; init; }
    public int FilesRemoved { get; init; }
    public int FilesSkipped { get; init; }
    public double GbFreed => BytesFreed / 1024.0 / 1024.0 / 1024.0;
}
