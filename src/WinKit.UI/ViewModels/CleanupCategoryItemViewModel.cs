using CommunityToolkit.Mvvm.ComponentModel;
using WinKit.Cleanup.Models;

namespace WinKit.UI.ViewModels;

public sealed partial class CleanupCategoryItemViewModel : ObservableObject
{
    public CleanupCategoryItemViewModel(CleanupCategoryScan scan, bool isSelected)
    {
        Scan = scan;
        _isSelected = isSelected;
    }

    public CleanupCategoryScan Scan { get; }

    [ObservableProperty]
    private bool _isSelected;
}
