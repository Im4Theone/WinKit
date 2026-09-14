namespace WinKit.SystemTools.Models;

public sealed class InstalledApp
{
    public required string Name { get; init; }
    public string? Publisher { get; init; }
    public string? Version { get; init; }
    public DateOnly? InstallDate { get; init; }
    public double? EstimatedSizeMb { get; init; }
    public string? UninstallCommand { get; init; }
}
