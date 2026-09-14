namespace WinKit.SystemTools.Models;

public enum StartupScope
{
    CurrentUserRegistry,
    AllUsersRegistry,
    CurrentUserFolder,
    AllUsersFolder
}

public sealed class StartupEntry
{
    public required string Name { get; init; }
    public string? Command { get; init; }
    public required StartupScope Scope { get; init; }
    public bool IsEnabled { get; init; }
    public bool RequiresElevationToChange => Scope is StartupScope.AllUsersRegistry or StartupScope.AllUsersFolder;
}
