namespace WinKit.UI.Dialogs;

public sealed class ConfirmationRequest
{
    public required string Title { get; init; }
    public required string Message { get; init; }
    public string ConfirmText { get; init; } = "Confirm";
    public string CancelText { get; init; } = "Cancel";
    public bool IsDestructive { get; init; }
}
