using WinKit.Cleanup.Models;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;

namespace WinKit.Cleanup.Services;

public sealed class CleanupExecutor : ICleanupExecutor
{
    private readonly IElevationService _elevationService;

    public CleanupExecutor(IElevationService elevationService)
    {
        _elevationService = elevationService;
    }

    public Task<OperationResult<CleanupSummary>> CleanAsync(
        IReadOnlyCollection<CleanupCategory> categories,
        IProgress<string>? progress = null,
        CancellationToken cancellationToken = default)
    {
        return Task.Run(() =>
        {
            long bytesFreed = 0;
            var removed = 0;
            var skipped = 0;

            foreach (var category in categories)
            {
                cancellationToken.ThrowIfCancellationRequested();

                switch (category)
                {
                    case CleanupCategory.UserTemp:
                        progress?.Report("Removing user temporary files...");
                        DeleteDirectoryContents(CleanupPaths.UserTemp, ref bytesFreed, ref removed, ref skipped);
                        break;

                    case CleanupCategory.WindowsTemp:
                        progress?.Report("Removing Windows temporary files...");
                        DeleteDirectoryContents(CleanupPaths.WindowsTemp, ref bytesFreed, ref removed, ref skipped);
                        break;

                    case CleanupCategory.ThumbnailCache:
                        progress?.Report("Clearing thumbnail cache...");
                        DeleteThumbnailCache(ref bytesFreed, ref removed, ref skipped);
                        break;

                    case CleanupCategory.RecycleBin:
                        progress?.Report("Emptying the Recycle Bin...");
                        var (size, _) = RecycleBinInterop.Query();
                        if (RecycleBinInterop.Empty())
                        {
                            bytesFreed += size;
                        }
                        break;

                    case CleanupCategory.WindowsUpdateCache:
                        progress?.Report("Removing Windows Update cache...");
                        DeleteDirectoryContents(
                            CleanupPaths.WindowsUpdateDownloadCache, ref bytesFreed, ref removed, ref skipped);
                        break;
                }
            }

            var summary = new CleanupSummary { BytesFreed = bytesFreed, FilesRemoved = removed, FilesSkipped = skipped };

            if (skipped == 0)
            {
                return OperationResult<CleanupSummary>.Ok(summary);
            }

            var skippedMessage = _elevationService.IsElevated
                ? $"{skipped} item(s) were in use and were skipped."
                : $"{skipped} item(s) were in use or protected and were skipped. " +
                  "To remove protected system files, go to Settings > Advanced > Run as Administrator.";

            return OperationResult<CleanupSummary>.Ok(summary, skippedMessage);
        }, cancellationToken);
    }

    private static void DeleteThumbnailCache(ref long bytesFreed, ref int removed, ref int skipped)
    {
        var path = CleanupPaths.ExplorerCache;
        if (!Directory.Exists(path))
        {
            return;
        }

        foreach (var file in Directory.EnumerateFiles(path, "*.db", SearchOption.TopDirectoryOnly))
        {
            var name = Path.GetFileName(file);
            if (!name.StartsWith("thumbcache_", StringComparison.OrdinalIgnoreCase) &&
                !name.StartsWith("iconcache_", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            TryDeleteFile(file, ref bytesFreed, ref removed, ref skipped);
        }
    }

    private static void DeleteDirectoryContents(string root, ref long bytesFreed, ref int removed, ref int skipped)
    {
        if (!Directory.Exists(root))
        {
            return;
        }

        IEnumerable<string> topLevelEntries;
        try
        {
            topLevelEntries = Directory.EnumerateFileSystemEntries(root).ToList();
        }
        catch (UnauthorizedAccessException)
        {
            return;
        }

        foreach (var entry in topLevelEntries)
        {
            if (Directory.Exists(entry))
            {
                DeleteDirectoryRecursive(entry, ref bytesFreed, ref removed, ref skipped);
            }
            else
            {
                TryDeleteFile(entry, ref bytesFreed, ref removed, ref skipped);
            }
        }
    }

    private static void DeleteDirectoryRecursive(string directory, ref long bytesFreed, ref int removed, ref int skipped)
    {
        string[] files;
        string[] subDirectories;
        try
        {
            files = Directory.GetFiles(directory);
            subDirectories = Directory.GetDirectories(directory);
        }
        catch (UnauthorizedAccessException)
        {
            skipped++;
            return;
        }
        catch (IOException)
        {
            skipped++;
            return;
        }

        foreach (var file in files)
        {
            TryDeleteFile(file, ref bytesFreed, ref removed, ref skipped);
        }

        foreach (var subDirectory in subDirectories)
        {
            DeleteDirectoryRecursive(subDirectory, ref bytesFreed, ref removed, ref skipped);
        }

        try
        {
            if (Directory.GetFileSystemEntries(directory).Length == 0)
            {
                Directory.Delete(directory);
            }
        }
        catch (IOException)
        {
            // Directory still has locked contents or is a mount point; leave it in place.
        }
        catch (UnauthorizedAccessException)
        {
            // Directory still has locked contents or is a mount point; leave it in place.
        }
    }

    private static void TryDeleteFile(string file, ref long bytesFreed, ref int removed, ref int skipped)
    {
        try
        {
            var length = new FileInfo(file).Length;
            File.SetAttributes(file, FileAttributes.Normal);
            File.Delete(file);
            bytesFreed += length;
            removed++;
        }
        catch (IOException)
        {
            skipped++;
        }
        catch (UnauthorizedAccessException)
        {
            skipped++;
        }
    }
}
