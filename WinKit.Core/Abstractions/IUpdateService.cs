using WinKit.Core.Models;

namespace WinKit.Core.Abstractions;

/// <summary>
/// Checks GitHub Releases for a newer WinKit version and, if the user chooses to
/// update, downloads and verifies the appropriate asset (installer or portable
/// zip, matching how this copy of WinKit is running) before handing off to a
/// separate process to actually replace files. Never replaces files itself.
/// </summary>
public interface IUpdateService
{
    Task<UpdateCheckResult> CheckForUpdateAsync(CancellationToken cancellationToken = default);

    Task<UpdateApplyResult> DownloadAndApplyUpdateAsync(UpdateCheckResult update, CancellationToken cancellationToken = default);
}
