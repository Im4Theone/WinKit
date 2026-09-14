using WinKit.Core.Models;

namespace WinKit.Core.Abstractions;

public interface IUserProfileService
{
    UserProfile Current { get; }

    event EventHandler? ProfileChanged;

    Task LoadAsync();

    Task SetNameAsync(string name);
}
