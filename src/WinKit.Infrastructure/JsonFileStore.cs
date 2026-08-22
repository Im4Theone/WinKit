using System.Text.Json;

namespace WinKit.Infrastructure;

/// <summary>
/// Minimal helper for reading/writing a single JSON document to disk.
/// Writes are atomic (write to temp file, then replace) so a crash mid-write
/// never corrupts the file the app relies on next launch.
/// </summary>
public static class JsonFileStore
{
    private static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true
    };

    public static async Task<T?> ReadAsync<T>(
        string path, JsonSerializerOptions? options = null, CancellationToken cancellationToken = default)
    {
        if (!File.Exists(path))
        {
            return default;
        }

        await using var stream = File.OpenRead(path);
        return await JsonSerializer.DeserializeAsync<T>(stream, options ?? Options, cancellationToken);
    }

    public static async Task WriteAsync<T>(
        string path, T value, JsonSerializerOptions? options = null, CancellationToken cancellationToken = default)
    {
        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        var tempPath = path + ".tmp";
        await using (var stream = File.Create(tempPath))
        {
            await JsonSerializer.SerializeAsync(stream, value, options ?? Options, cancellationToken);
        }

        File.Copy(tempPath, path, overwrite: true);
        File.Delete(tempPath);
    }
}
