using WinKit.Core.Models;
using WinKit.Network.Services;

namespace WinKit.Diagnostics.Fixers;

/// <summary>Delegates to the existing Network Maintenance service rather than re-invoking ipconfig itself.</summary>
public sealed class DnsFlushFixer : IDiagnosticFixer
{
    private readonly INetworkMaintenanceService _networkMaintenanceService;

    public DnsFlushFixer(INetworkMaintenanceService networkMaintenanceService)
    {
        _networkMaintenanceService = networkMaintenanceService;
    }

    public string CheckId => DiagnosticCheckIds.DnsResolution;

    public async Task<OperationResult<string>> FixAsync(IProgress<string>? progress, CancellationToken cancellationToken)
    {
        progress?.Report("Flushing DNS cache...");

        var result = await _networkMaintenanceService.FlushDnsAsync(cancellationToken);
        return result.Success
            ? OperationResult<string>.Ok("DNS cache flushed.")
            : OperationResult<string>.Fail(result.UserMessage ?? "Unable to flush the DNS cache.", result.TechnicalDetail);
    }
}
