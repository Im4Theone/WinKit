using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.UI.Navigation;

namespace WinKit.UI.ViewModels;

public sealed partial class ShellViewModel : ObservableObject
{
    private readonly INavigationService _navigationService;

    public ShellViewModel(INavigationService navigationService, NotificationHostViewModel notificationHost)
    {
        _navigationService = navigationService;
        NotificationHost = notificationHost;

        NavItems = new List<NavItem>
        {
            new() { Title = "Dashboard", Glyph = "Icon.Dashboard", TargetViewModelType = typeof(DashboardViewModel) },
            new() { Title = "Tools", IsHeader = true },
            new() { Title = "System", Glyph = "Icon.System", TargetViewModelType = typeof(SystemToolsViewModel) },
            new() { Title = "Network", Glyph = "Icon.Network", TargetViewModelType = typeof(NetworkToolsViewModel) },
            new() { Title = "Cleanup", Glyph = "Icon.Cleanup", TargetViewModelType = typeof(CleanupViewModel) },
            new() { Title = "Diagnostics", Glyph = "Icon.Diagnostics", TargetViewModelType = typeof(DiagnosticsViewModel) },
            new() { Title = "App", IsHeader = true },
            new() { Title = "Settings", Glyph = "Icon.Settings", TargetViewModelType = typeof(SettingsViewModel) },
            new() { Title = "About", Glyph = "Icon.About", TargetViewModelType = typeof(AboutViewModel) },
        };

        _navigationService.Navigated += (_, _) =>
        {
            var currentType = _navigationService.CurrentViewModel?.GetType();
            foreach (var navItem in NavItems)
            {
                navItem.IsSelected = navItem.TargetViewModelType == currentType;
            }

            OnPropertyChanged(nameof(CurrentViewModel));
            OnPropertyChanged(nameof(ActiveOnboarding));
            OnPropertyChanged(nameof(ShowChrome));
        };
    }

    public List<NavItem> NavItems { get; }

    public NotificationHostViewModel NotificationHost { get; }

    public ObservableObject? CurrentViewModel => _navigationService.CurrentViewModel;

    /// <summary>Non-null only while onboarding is the active page; drives the chrome-free full-window layout.</summary>
    public OnboardingViewModel? ActiveOnboarding => CurrentViewModel as OnboardingViewModel;

    public bool ShowChrome => ActiveOnboarding is null;

    [RelayCommand]
    private void Select(NavItem item)
    {
        if (item.TargetViewModelType is not null)
        {
            _navigationService.NavigateTo(item.TargetViewModelType);
        }
    }
}
