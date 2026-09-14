namespace WinKit.UI.Navigation;

/// <summary>Implemented by page view models that load data the first time they're navigated to.</summary>
public interface IAsyncInitializable
{
    Task InitializeAsync();
}
