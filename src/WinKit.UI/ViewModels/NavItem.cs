using CommunityToolkit.Mvvm.ComponentModel;

namespace WinKit.UI.ViewModels;

public sealed partial class NavItem : ObservableObject
{
    public required string Title { get; init; }
    public string? Glyph { get; init; }
    public Type? TargetViewModelType { get; init; }
    public bool IsHeader { get; init; }

    /// <summary>False for group headers, which are labels rather than clickable/focusable nav targets.</summary>
    public bool IsSelectable => !IsHeader;

    [ObservableProperty]
    private bool _isSelected;
}
