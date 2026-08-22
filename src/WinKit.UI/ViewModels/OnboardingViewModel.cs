using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Core.Abstractions;
using WinKit.UI.Navigation;

namespace WinKit.UI.ViewModels;

public sealed partial class OnboardingViewModel : ObservableObject
{
    private readonly IUserProfileService _profileService;
    private readonly INavigationService _navigationService;

    public OnboardingViewModel(IUserProfileService profileService, INavigationService navigationService)
    {
        _profileService = profileService;
        _navigationService = navigationService;
    }

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(ContinueCommand))]
    private string _name = string.Empty;

    [RelayCommand(CanExecute = nameof(CanContinue))]
    private async Task ContinueAsync()
    {
        await _profileService.SetNameAsync(Name);
        _navigationService.NavigateTo<DashboardViewModel>();
    }

    private bool CanContinue() => !string.IsNullOrWhiteSpace(Name);
}
