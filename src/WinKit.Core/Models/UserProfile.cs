namespace WinKit.Core.Models;

public sealed class UserProfile
{
    public string Name { get; set; } = string.Empty;

    public bool HasCompletedOnboarding => !string.IsNullOrWhiteSpace(Name);
}
