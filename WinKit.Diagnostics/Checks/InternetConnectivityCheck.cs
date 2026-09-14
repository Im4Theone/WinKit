using System.Net.NetworkInformation;
using WinKit.Core.Models;
using WinKit.Network.Services;
using PingReply = WinKit.Network.Models.PingReply;

namespace WinKit.Diagnostics.Checks;

/// <summary>Reuses the interactive Ping tool's service for a single-probe connectivity check.</summary>
public sealed class InternetConnectivityCheck : IDiagnosticCheck
{
    private const string Target = "1.1.1.1";

    private readonly IPingService _pingService;

    public InternetConnectivityCheck(IPingService pingService)
    {
        _pingService = pingService;
    }

    public string Id => DiagnosticCheckIds.InternetConnectivity;
    public string Name => "Internet Connectivity";
    public DiagnosticCategory Category => DiagnosticCategory.Network;

    public async Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        PingReply? reply = null;
        var progress = new Progress<PingReply>(r => reply = r);

        // PingService already swallows PingException per-attempt and reports it as
        // IPStatus.Unknown, so a failed ping surfaces here as a normal reply, not a throw.
        await _pingService.PingAsync(Target, count: 1, progress, cancellationToken);

        if (reply is { Status: IPStatus.Success })
        {
            return new DiagnosticCheckResult
            {
                CheckId = Id,
                Name = Name,
                Category = Category,
                Status = DiagnosticStatus.Passed,
                Summary = $"Reached {Target} in {reply.RoundTripTimeMs} ms."
            };
        }

        return Failed($"Ping to {Target} returned {reply?.Status.ToString() ?? "no response"}.");
    }

    private DiagnosticCheckResult Failed(string detail) => new()
    {
        CheckId = Id,
        Name = Name,
        Category = Category,
        Status = DiagnosticStatus.Failed,
        Severity = DiagnosticSeverity.Medium,
        Summary = "Your computer could not reach the internet.",
        TechnicalDetail = detail,
        RecommendedAction = "Check your Wi-Fi or ethernet connection, or contact your network administrator."
    };
}
