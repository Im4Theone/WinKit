using WinKit.Core.Models;

namespace WinKit.Diagnostics;

/// <summary>
/// The repair/action logic for one check, kept separate from detection so a check
/// can be read-only while its remedy is independently testable and swappable.
/// The engine matches a fixer to a check result by CheckId.
/// </summary>
public interface IDiagnosticFixer
{
    /// <summary>The DiagnosticCheckIds value this fixer applies to.</summary>
    string CheckId { get; }

    Task<OperationResult<string>> FixAsync(IProgress<string>? progress, CancellationToken cancellationToken);
}
