namespace WinKit.Core.Models;

public enum UpdateCheckStatus
{
    UpToDate,
    UpdateAvailable,
    Failed
}

/// <summary>
/// Result of checking GitHub Releases for a newer WinKit version. Asset URLs are
/// populated only when an update is available and the release actually contains
/// that asset type (installer vs. portable zip, plus its .sha256 checksum).
/// </summary>
public sealed class UpdateCheckResult
{
    public required UpdateCheckStatus Status { get; init; }
    public string? LatestVersion { get; init; }
    public string? ReleaseUrl { get; init; }
    public string? ReleaseNotes { get; init; }
    public string? InstallerAssetUrl { get; init; }
    public string? InstallerChecksumAssetUrl { get; init; }
    public string? PortableAssetUrl { get; init; }
    public string? PortableChecksumAssetUrl { get; init; }
    public string? ErrorMessage { get; init; }
}

public enum UpdateApplyStatus
{
    /// <summary>The installer/updater helper process was launched successfully; the caller should now exit WinKit.</summary>
    Started,
    Failed
}

public sealed class UpdateApplyResult
{
    public required UpdateApplyStatus Status { get; init; }
    public string? ErrorMessage { get; init; }
}
