using CommunityToolkit.Mvvm.ComponentModel;

namespace WinKit.UI.Navigation;

public interface INavigationService
{
    ObservableObject? CurrentViewModel { get; }

    event EventHandler? Navigated;

    TViewModel NavigateTo<TViewModel>() where TViewModel : ObservableObject;

    ObservableObject NavigateTo(Type viewModelType);
}
