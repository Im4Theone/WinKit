using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public interface IHostsFileService
{
    string HostsFilePath { get; }

    Task<string> ReadRawAsync(CancellationToken cancellationToken = default);

    Task<IReadOnlyList<HostsEntry>> ParseEntriesAsync(CancellationToken cancellationToken = default);

    Task<OperationResult> WriteRawAsync(string content, CancellationToken cancellationToken = default);
}
