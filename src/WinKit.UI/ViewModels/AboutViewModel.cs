using System.Reflection;
using CommunityToolkit.Mvvm.ComponentModel;

namespace WinKit.UI.ViewModels;

public sealed class AboutViewModel : ObservableObject
{
    public string Version { get; } =
        Assembly.GetExecutingAssembly().GetName().Version?.ToString(3) ?? "1.0.0";
}
