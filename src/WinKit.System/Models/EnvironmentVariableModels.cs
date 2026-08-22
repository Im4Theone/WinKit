namespace WinKit.SystemTools.Models;

public enum EnvironmentVariableScope
{
    User,
    Machine
}

public sealed class EnvironmentVariableEntry
{
    public required string Name { get; init; }
    public required string Value { get; init; }
    public required EnvironmentVariableScope Scope { get; init; }
}
