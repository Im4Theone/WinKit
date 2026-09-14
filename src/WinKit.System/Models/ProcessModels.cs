namespace WinKit.SystemTools.Models;

public sealed class ProcessEntry
{
    public required int Id { get; init; }
    public required string Name { get; init; }
    public string? FilePath { get; init; }
    public long WorkingSetBytes { get; init; }
    public double CpuPercent { get; init; }
    public int ThreadCount { get; init; }
    public bool CanBeEnded { get; init; }
}
