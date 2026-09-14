using CommunityToolkit.Mvvm.ComponentModel;

namespace WinKit.UI.ViewModels;

/// <summary>One entry in a tool page's internal tab strip (e.g. System > Process Manager).</summary>
public sealed partial class ToolTabItem : ObservableObject
{
    private readonly Func<Task>? _onFirstSelected;
    private bool _loaded;

    public ToolTabItem(string title, ObservableObject viewModel, Func<Task>? onFirstSelected = null)
    {
        Title = title;
        ViewModel = viewModel;
        _onFirstSelected = onFirstSelected;
    }

    public string Title { get; }
    public ObservableObject ViewModel { get; }

    [ObservableProperty]
    private bool _isSelected;

    public async Task EnsureLoadedAsync()
    {
        if (_loaded || _onFirstSelected is null)
        {
            return;
        }

        _loaded = true;
        await _onFirstSelected();
    }
}
