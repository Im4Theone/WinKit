using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace WinKit.UI.ViewModels;

/// <summary>Common shape for a tool page made of an internal tab strip (System, Network, etc.).</summary>
public abstract partial class ToolPageViewModelBase : ObservableObject
{
    protected ToolPageViewModelBase(IEnumerable<ToolTabItem> tabs)
    {
        Tabs = tabs.ToList();
    }

    public List<ToolTabItem> Tabs { get; }

    [ObservableProperty]
    private ToolTabItem? _selectedTab;

    partial void OnSelectedTabChanged(ToolTabItem? oldValue, ToolTabItem? newValue)
    {
        if (oldValue is not null)
        {
            oldValue.IsSelected = false;
        }

        if (newValue is not null)
        {
            newValue.IsSelected = true;
            _ = newValue.EnsureLoadedAsync();
        }
    }

    [RelayCommand]
    private void SelectTab(ToolTabItem tab) => SelectedTab = tab;

    protected void SelectFirstTab() => SelectedTab = Tabs.FirstOrDefault();
}
