using WinKit.Core.Models;

namespace WinKit.Diagnostics.Checks;

public sealed class DnsResolutionCheck : IDiagnosticCheck
{
    private const string ProbeHost = "www.microsoft.com";

    public string Id => DiagnosticCheckIds.DnsResolution;
    public string Name => "DNS Resolution";
    public DiagnosticCategory Category => DiagnosticCategory.Network;

    public async Task<DiagnosticCheckResult> RunAsync(CancellationToken cancellationToken)
    {
        try
        {
            var entry = await System.Net.Dns.GetHostEntryAsync(ProbeHost, cancellationToken);
            if (entry.AddressList.Length > 0)
            {
                return new DiagnosticCheckResult
                {
                    CheckId = Id,
                    Name = Name,
                    Category = Category,
                    Status = DiagnosticStatus.Passed,
                    Summary = "DNS lookups are working."
                };
            }

            return Failed($"DNS lookup for {ProbeHost} returned no addresses.");
        }
        catch (System.Net.Sockets.SocketException ex)
        {
            return Failed(ex.Message);
        }
    }

    private DiagnosticCheckResult Failed(string detail) => new()
    {
        CheckId = Id,
        Name = Name,
        Category = Category,
        Status = DiagnosticStatus.Failed,
        Severity = DiagnosticSeverity.Medium,
        Summary = "DNS lookups are failing.",
        TechnicalDetail = detail,
        RecommendedAction = "Flush the DNS cache, or check your DNS server configuration.",
        FixAction = new DiagnosticFixAction
        {
            Label = "Flush DNS Cache",
            Kind = DiagnosticActionKind.Fix,
            ConfirmationMessage = "This clears your computer's cached DNS lookups."
        }
    };
}
