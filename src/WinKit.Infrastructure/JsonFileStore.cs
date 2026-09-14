using System.Collections.Concurrent;
using System.Text.Json;

namespace WinKit.Infrastructure;

/// <summary>
/// Minimal helper for reading/writing a single JSON document to disk.
/// Writes are serialized per-path and go through a uniquely-named temp file
/// plus an atomic swap, so concurrent writers to the same file never race
/// and a crash mid-write never corrupts the file the app relies on next
/// launch. Reads that hit unparsable JSON move the bad file aside instead
/// of throwing, so a corrupt file degrades to "use defaults" rather than
/// blocking startup.
/// </summary>
public static class JsonFileStore
{
    private static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true
    };

    private static readonly ConcurrentDictionary<string, SemaphoreSlim> Locks = new(StringComparer.OrdinalIgnoreCase);

    public static async Task<T?> ReadAsync<T>(
        string path, JsonSerializerOptions? options = null, CancellationToken cancellationToken = default)
    {
        var fullPath = Path.GetFullPath(path);
        var gate = GetLock(fullPath);
        await gate.WaitAsync(cancellationToken);
        try
        {
            if (!File.Exists(fullPath))
            {
                return default;
            }

            try
            {
                await using var stream = File.OpenRead(fullPath);
                return await JsonSerializer.DeserializeAsync<T>(stream, options ?? Options, cancellationToken);
            }
            catch (Exception ex) when (ex is JsonException or IOException)
            {
                QuarantineCorruptFile(fullPath, ex);
                return default;
            }
        }
        finally
        {
            gate.Release();
        }
    }

    public static async Task WriteAsync<T>(
        string path, T value, JsonSerializerOptions? options = null, CancellationToken cancellationToken = default)
    {
        var fullPath = Path.GetFullPath(path);
        var directory = Path.GetDirectoryName(fullPath);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        var gate = GetLock(fullPath);
        await gate.WaitAsync(cancellationToken);
        try
        {
            // A unique name per attempt means no two concurrent writers, and no
            // orphaned temp file left by a killed process, can ever collide.
            var tempPath = fullPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                await using (var stream = File.Create(tempPath))
                {
                    await JsonSerializer.SerializeAsync(stream, value, options ?? Options, cancellationToken);
                    await stream.FlushAsync(cancellationToken);
                }

                // File.Replace/Move perform an atomic filesystem rename, unlike
                // Copy+Delete: the destination is either fully the old content or
                // fully the new content, never a partial/missing state in between.
                if (File.Exists(fullPath))
                {
                    File.Replace(tempPath, fullPath, null);
                }
                else
                {
                    File.Move(tempPath, fullPath);
                }
            }
            finally
            {
                if (File.Exists(tempPath))
                {
                    try
                    {
                        File.Delete(tempPath);
                    }
                    catch (IOException)
                    {
                        // Best-effort cleanup; an orphaned uniquely-named temp file is harmless.
                    }
                }
            }
        }
        finally
        {
            gate.Release();
        }
    }

    private static SemaphoreSlim GetLock(string fullPath) =>
        Locks.GetOrAdd(fullPath, static _ => new SemaphoreSlim(1, 1));

    private static void QuarantineCorruptFile(string fullPath, Exception cause)
    {
        try
        {
            var quarantinePath = $"{fullPath}.corrupt-{DateTime.UtcNow:yyyyMMddHHmmss}.json";
            File.Move(fullPath, quarantinePath, overwrite: true);
            LogRecovery(fullPath, quarantinePath, cause);
        }
        catch (IOException)
        {
            // If even quarantining fails, leave the original file in place; the
            // caller still gets `default` back and the app falls back to defaults.
        }
    }

    private static void LogRecovery(string originalPath, string quarantinePath, Exception cause)
    {
        try
        {
            AppPaths.EnsureCreated();
            File.AppendAllText(
                Path.Combine(AppPaths.LogsDirectory, "crash.log"),
                $"{DateTime.Now:O}{Environment.NewLine}" +
                $"Recovered from unreadable file: {originalPath}{Environment.NewLine}" +
                $"Quarantined to: {quarantinePath}{Environment.NewLine}" +
                $"{cause}{Environment.NewLine}{new string('-', 40)}{Environment.NewLine}");
        }
        catch (IOException)
        {
            // Best-effort logging; never let logging itself throw.
        }
    }
}
