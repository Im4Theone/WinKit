using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public interface ISystemInfoService
{
    Task<SystemOverview> GetOverviewAsync(CancellationToken cancellationToken = default);
}
