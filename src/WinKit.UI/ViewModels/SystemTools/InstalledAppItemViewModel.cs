using CommunityToolkit.Mvvm.ComponentModel;
using WinKit.SystemTools.Models;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class InstalledAppItemViewModel : ObservableObject
{
    public InstalledAppItemViewModel(InstalledApp app)
    {
        App = app;
    }

    public InstalledApp App { get; }

    [ObservableProperty]
    private bool _isUninstalling;
}
