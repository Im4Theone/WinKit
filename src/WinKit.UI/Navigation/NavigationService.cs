using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace WinKit.UI.Navigation;

/// <summary>
/// ViewModel-first navigation: the shell binds its content to CurrentViewModel
/// and implicit DataTemplates (keyed by view model type) pick the matching view.
/// Instances are cached per type so switching tabs doesn't lose page state.
/// </summary>
public sealed class NavigationService : INavigationService
{
    private readonly IServiceProvider _services;
    private readonly ILogger<NavigationService> _logger;
    private readonly Dictionary<Type, ObservableObject> _cache = new();

    public NavigationService(IServiceProvider services, ILogger<NavigationService> logger)
    {
        _services = services;
        _logger = logger;
    }

    public ObservableObject? CurrentViewModel { get; private set; }

    public event EventHandler? Navigated;

    public TViewModel NavigateTo<TViewModel>() where TViewModel : ObservableObject =>
        (TViewModel)NavigateTo(typeof(TViewModel));

    public ObservableObject NavigateTo(Type viewModelType)
    {
        if (!_cache.TryGetValue(viewModelType, out var viewModel))
        {
            viewModel = (ObservableObject)_services.GetRequiredService(viewModelType);
            _cache[viewModelType] = viewModel;

            if (viewModel is IAsyncInitializable initializable)
            {
                _ = InitializeSafelyAsync(initializable);
            }
        }

        CurrentViewModel = viewModel;
        Navigated?.Invoke(this, EventArgs.Empty);
        return viewModel;
    }

    private async Task InitializeSafelyAsync(IAsyncInitializable initializable)
    {
        try
        {
            await initializable.InitializeAsync();
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to initialize page view model {Type}", initializable.GetType().Name);
        }
    }
}
