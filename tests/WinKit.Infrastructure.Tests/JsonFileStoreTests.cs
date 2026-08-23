using WinKit.Infrastructure;
using Xunit;

namespace WinKit.Infrastructure.Tests;

public sealed class JsonFileStoreTests : IDisposable
{
    private sealed record Sample(string Name, int Count);

    private readonly string _directory;

    public JsonFileStoreTests()
    {
        _directory = Path.Combine(Path.GetTempPath(), "WinKitTests_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_directory);
    }

    public void Dispose()
    {
        try
        {
            Directory.Delete(_directory, recursive: true);
        }
        catch (IOException)
        {
            // Best-effort cleanup.
        }
    }

    private string PathFor(string fileName) => Path.Combine(_directory, fileName);

    [Fact]
    public async Task WriteAsync_ThenReadAsync_RoundTripsValue()
    {
        var path = PathFor("normal.json");
        var value = new Sample("Midnight", 3);

        await JsonFileStore.WriteAsync(path, value);
        var result = await JsonFileStore.ReadAsync<Sample>(path);

        Assert.Equal(value, result);
    }

    [Fact]
    public async Task WriteAsync_CreatesMissingDirectory()
    {
        var nestedPath = Path.Combine(_directory, "nested", "sub", "settings.json");

        await JsonFileStore.WriteAsync(nestedPath, new Sample("A", 1));

        Assert.True(File.Exists(nestedPath));
    }

    [Fact]
    public async Task WriteAsync_LeavesNoTempFilesBehindOnSuccess()
    {
        var path = PathFor("clean.json");

        await JsonFileStore.WriteAsync(path, new Sample("A", 1));

        var leftovers = Directory.GetFiles(_directory, "*.tmp");
        Assert.Empty(leftovers);
    }

    [Fact]
    public async Task ConcurrentWrites_ToSamePath_AllSucceedAndFileEndsUpValid()
    {
        var path = PathFor("concurrent.json");

        var writers = Enumerable.Range(0, 25)
            .Select(i => JsonFileStore.WriteAsync(path, new Sample($"Writer{i}", i)))
            .ToArray();

        // None of the concurrent writers should throw (this is the exact shape of
        // the reported bug: two writers racing on a shared temp filename).
        await Task.WhenAll(writers);

        var result = await JsonFileStore.ReadAsync<Sample>(path);
        Assert.NotNull(result);

        var leftovers = Directory.GetFiles(_directory, "*.tmp");
        Assert.Empty(leftovers);
    }

    [Fact]
    public async Task ConcurrentReadsAndWrites_ToSamePath_NeverThrow()
    {
        var path = PathFor("mixed.json");
        await JsonFileStore.WriteAsync(path, new Sample("Seed", 0));

        var operations = new List<Task>();
        for (var i = 0; i < 15; i++)
        {
            var index = i;
            operations.Add(JsonFileStore.WriteAsync(path, new Sample($"W{index}", index)));
            operations.Add(JsonFileStore.ReadAsync<Sample>(path));
        }

        await Task.WhenAll(operations);
    }

    [Fact]
    public async Task ReadAsync_MissingFile_ReturnsDefault()
    {
        var path = PathFor("does-not-exist.json");

        var result = await JsonFileStore.ReadAsync<Sample>(path);

        Assert.Null(result);
    }

    [Fact]
    public async Task ReadAsync_CorruptJson_ReturnsDefaultAndQuarantinesFile()
    {
        var path = PathFor("corrupt.json");
        await File.WriteAllTextAsync(path, "{ this is not valid json ");

        var result = await JsonFileStore.ReadAsync<Sample>(path);

        Assert.Null(result);
        Assert.False(File.Exists(path));

        var quarantined = Directory.GetFiles(_directory, "corrupt.json.corrupt-*.json");
        Assert.Single(quarantined);
    }

    [Fact]
    public async Task WriteAsync_AfterPriorCorruptRead_SucceedsNormally()
    {
        var path = PathFor("recover.json");
        await File.WriteAllTextAsync(path, "not json at all");

        var readResult = await JsonFileStore.ReadAsync<Sample>(path);
        Assert.Null(readResult);

        await JsonFileStore.WriteAsync(path, new Sample("Recovered", 7));
        var result = await JsonFileStore.ReadAsync<Sample>(path);

        Assert.Equal(new Sample("Recovered", 7), result);
    }

    [Fact]
    public async Task WriteAsync_SimulatedInterruption_NeverLeavesDestinationMissingOrPartial()
    {
        var path = PathFor("interrupted.json");
        await JsonFileStore.WriteAsync(path, new Sample("Original", 1));

        // Simulate a crash mid-write: a temp file exists (from some previous
        // attempt) but was never swapped in. It must not interfere with the
        // real file, and a subsequent write must still succeed.
        var orphanTemp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        await File.WriteAllTextAsync(orphanTemp, "{\"Name\":\"Orphan\",\"Count\":99}");

        var beforeRead = await JsonFileStore.ReadAsync<Sample>(path);
        Assert.Equal(new Sample("Original", 1), beforeRead);

        await JsonFileStore.WriteAsync(path, new Sample("Updated", 2));
        var afterRead = await JsonFileStore.ReadAsync<Sample>(path);
        Assert.Equal(new Sample("Updated", 2), afterRead);

        // The orphaned temp file from the "crash" is never cleaned up by
        // WriteAsync (it doesn't know about temp files it didn't create), but
        // it must never have been mistaken for the real file.
        File.Delete(orphanTemp);
    }
}
