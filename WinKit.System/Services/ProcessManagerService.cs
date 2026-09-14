using System.Diagnostics;
using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public sealed class ProcessManagerService : IProcessManagerService
{
    public async Task<IReadOnlyList<ProcessEntry>> GetProcessesAsync(CancellationToken cancellationToken = default)
    {
        var processes = Process.GetProcesses();

        var samples = new Dictionary<int, TimeSpan>();
        foreach (var process in processes)
        {
            samples[process.Id] = TryGetCpuTime(process);
        }

        var sampleStart = DateTime.UtcNow;
        await Task.Delay(300, cancellationToken);
        var elapsed = (DateTime.UtcNow - sampleStart).TotalMilliseconds;
        var coreCount = Environment.ProcessorCount;

        var results = new List<ProcessEntry>(processes.Length);
        foreach (var process in processes)
        {
            try
            {
                var previousCpuTime = samples.GetValueOrDefault(process.Id);
                var currentCpuTime = TryGetCpuTime(process);
                var cpuDeltaMs = (currentCpuTime - previousCpuTime).TotalMilliseconds;
                var cpuPercent = elapsed <= 0 ? 0 : Math.Clamp(cpuDeltaMs / elapsed / coreCount * 100.0, 0, 100);

                string? filePath = null;
                try
                {
                    filePath = process.MainModule?.FileName;
                }
                catch (Exception ex) when (ex is InvalidOperationException or System.ComponentModel.Win32Exception)
                {
                    // Access denied reading module info for elevated/system processes.
                }

                results.Add(new ProcessEntry
                {
                    Id = process.Id,
                    Name = process.ProcessName,
                    FilePath = filePath,
                    WorkingSetBytes = process.WorkingSet64,
                    CpuPercent = cpuPercent,
                    ThreadCount = process.Threads.Count,
                    CanBeEnded = process.Id != Environment.ProcessId
                });
            }
            catch (InvalidOperationException)
            {
                // Process exited while we were inspecting it.
            }
            finally
            {
                process.Dispose();
            }
        }

        return results.OrderByDescending(p => p.CpuPercent).ThenByDescending(p => p.WorkingSetBytes).ToList();
    }

    public OperationResult EndProcess(int processId)
    {
        try
        {
            using var process = Process.GetProcessById(processId);
            process.Kill();
            return OperationResult.Ok();
        }
        catch (ArgumentException)
        {
            return OperationResult.Fail("That process has already exited.");
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            return OperationResult.Fail("Unable to end that process. It may require administrator access.", ex.Message);
        }
        catch (InvalidOperationException ex)
        {
            return OperationResult.Fail("Unable to end that process.", ex.Message);
        }
    }

    private static TimeSpan TryGetCpuTime(Process process)
    {
        try
        {
            return process.TotalProcessorTime;
        }
        catch (System.ComponentModel.Win32Exception)
        {
            return TimeSpan.Zero;
        }
        catch (InvalidOperationException)
        {
            return TimeSpan.Zero;
        }
    }
}
