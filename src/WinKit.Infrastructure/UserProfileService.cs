using WinKit.Core.Abstractions;
using WinKit.Core.Models;

namespace WinKit.Infrastructure;

public sealed class UserProfileService : IUserProfileService
{
    private UserProfile _current = new();

    public UserProfile Current => _current;

    public event EventHandler? ProfileChanged;

    public async Task LoadAsync()
    {
        _current = await JsonFileStore.ReadAsync<UserProfile>(AppPaths.ProfileFile) ?? new UserProfile();
        ProfileChanged?.Invoke(this, EventArgs.Empty);
    }

    public async Task SetNameAsync(string name)
    {
        _current = new UserProfile { Name = name.Trim() };
        await JsonFileStore.WriteAsync(AppPaths.ProfileFile, _current);
        ProfileChanged?.Invoke(this, EventArgs.Empty);
    }
}
