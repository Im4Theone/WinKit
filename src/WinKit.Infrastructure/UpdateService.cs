using System.Diagnostics;
using System.IO.Compression;
using System.Net.Http.Json;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;

namespace WinKit.Infrastructure;

public sealed class UpdateService : IUpdateService
{
    private const string ReleasesApiUrl = "https://api.github.com/repos/Im4Theone/WinKit/releases/latest";

    private readonly HttpClient _httpClient;

    public UpdateService(HttpClient httpClient)
    {
        _httpClient = httpClient;
    }

    public async Task<UpdateCheckResult> CheckForUpdateAsync(CancellationToken cancellationToken = default)
    {
        try
        {
            using var request = new HttpRequestMessage(HttpMethod.Get, ReleasesApiUrl);
            request.Headers.UserAgent.ParseAdd($"WinKit/{GetCurrentVersionText()}");
            request.Headers.Accept.ParseAdd("application/vnd.github+json");

            using var response = await _httpClient.SendAsync(request, cancellationToken);
            if (!response.IsSuccessStatusCode)
            {
                return Failed($"GitHub responded with {(int)response.StatusCode} {response.ReasonPhrase}.");
            }

            var release = await response.Content.ReadFromJsonAsync<GitHubRelease>(cancellationToken: cancellationToken);
            if (string.IsNullOrEmpty(release?.TagName))
            {
                return Failed("GitHub returned an unexpected response.");
            }

            var latestVersionText = release.TagName.TrimStart('v', 'V');
            if (!Version.TryParse(latestVersionText, out var latestVersion))
            {
                return Failed("Couldn't parse the latest release version.");
            }

            if (latestVersion <= GetCurrentVersion())
            {
                return new UpdateCheckResult { Status = UpdateCheckStatus.UpToDate };
            }

            var assets = release.Assets ?? new List<GitHubAsset>();
            var installer = FindAsset(assets, n => n.StartsWith("WinKitSetup-", StringComparison.OrdinalIgnoreCase) && n.EndsWith(".exe", StringComparison.OrdinalIgnoreCase));
            var portable = FindAsset(assets, n => n.StartsWith("WinKit-", StringComparison.OrdinalIgnoreCase) && n.EndsWith("-portable.zip", StringComparison.OrdinalIgnoreCase));

            return new UpdateCheckResult
            {
                Status = UpdateCheckStatus.UpdateAvailable,
                LatestVersion = latestVersionText,
                ReleaseUrl = release.HtmlUrl,
                ReleaseNotes = release.Body,
                InstallerAssetUrl = installer?.BrowserDownloadUrl,
                InstallerSha256 = ParseSha256Digest(installer?.Digest),
                PortableAssetUrl = portable?.BrowserDownloadUrl,
                PortableSha256 = ParseSha256Digest(portable?.Digest)
            };
        }
        catch (HttpRequestException ex)
        {
            return Failed($"Couldn't reach GitHub ({ex.Message}).");
        }
        catch (TaskCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            return Failed("The update check timed out.");
        }
        catch (JsonException)
        {
            return Failed("GitHub returned an unexpected response.");
        }
    }

    public async Task<UpdateApplyResult> DownloadAndApplyUpdateAsync(UpdateCheckResult update, CancellationToken cancellationToken = default)
    {
        try
        {
            return IsRunningFromInstalledLocation()
                ? await ApplyViaInstallerAsync(update, cancellationToken)
                : await ApplyViaPortableUpdaterAsync(update, cancellationToken);
        }
        catch (HttpRequestException ex)
        {
            return FailedApply($"Couldn't download the update ({ex.Message}).");
        }
        catch (TaskCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            return FailedApply("The download timed out.");
        }
        catch (IOException ex)
        {
            return FailedApply($"Couldn't prepare the update ({ex.Message}).");
        }
        catch (UnauthorizedAccessException)
        {
            return FailedApply("WinKit doesn't have permission to prepare the update.");
        }
    }

    private async Task<UpdateApplyResult> ApplyViaInstallerAsync(UpdateCheckResult update, CancellationToken cancellationToken)
    {
        if (update.InstallerAssetUrl is null || update.InstallerSha256 is null)
        {
            return FailedApply("This release doesn't include an installer download.");
        }

        var installerPath = await DownloadToTempFileAsync(update.InstallerAssetUrl, $"WinKitSetup-{update.LatestVersion}.exe", cancellationToken);
        if (!await VerifyChecksumAsync(installerPath, update.InstallerSha256, cancellationToken))
        {
            TryDelete(installerPath);
            return FailedApply("The downloaded installer failed checksum verification.");
        }

        // The installer runs as its own elevated process and replaces WinKit's files
        // after WinKit exits — this call never touches the running app's own files.
        // It's launched off the calling thread because this is invoked directly from
        // a UI command: an installer that self-elevates via its manifest blocks
        // ShellExecuteEx until the UAC prompt is resolved, which would otherwise
        // freeze WinKit for as long as the prompt is on screen.
        await Task.Run(() => Process.Start(new ProcessStartInfo
        {
            FileName = installerPath,
            Arguments = "/VERYSILENT /NORESTART /launchafterinstall=1",
            UseShellExecute = true
        }), cancellationToken);

        return new UpdateApplyResult { Status = UpdateApplyStatus.Started };
    }

    private async Task<UpdateApplyResult> ApplyViaPortableUpdaterAsync(UpdateCheckResult update, CancellationToken cancellationToken)
    {
        if (update.PortableAssetUrl is null || update.PortableSha256 is null)
        {
            return FailedApply("This release doesn't include a portable download.");
        }

        var installDir = AppContext.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar);
        var updaterPath = Path.Combine(installDir, "WinKit.Updater.exe");
        if (!File.Exists(updaterPath))
        {
            return FailedApply("The updater helper is missing from this installation.");
        }

        var zipPath = await DownloadToTempFileAsync(update.PortableAssetUrl, $"WinKit-{update.LatestVersion}-portable.zip", cancellationToken);
        if (!await VerifyChecksumAsync(zipPath, update.PortableSha256, cancellationToken))
        {
            TryDelete(zipPath);
            return FailedApply("The downloaded update failed checksum verification.");
        }

        var stagingDir = Path.Combine(Path.GetTempPath(), "WinKitUpdate", "staging-" + Guid.NewGuid().ToString("N"));
        ZipFile.ExtractToDirectory(zipPath, stagingDir);
        TryDelete(zipPath);

        // WinKit.Updater is a separate small process: it waits for this process to
        // exit, copies the staged files over installDir, then relaunches WinKit.exe.
        // Nothing here touches WinKit's own running files. Launched off the calling
        // thread for the same reason as the installer path above.
        await Task.Run(() => Process.Start(new ProcessStartInfo
        {
            FileName = updaterPath,
            Arguments = $"\"{stagingDir}\" \"{installDir}\" {Environment.ProcessId}",
            UseShellExecute = true,
            WorkingDirectory = installDir
        }), cancellationToken);

        return new UpdateApplyResult { Status = UpdateApplyStatus.Started };
    }

    private static bool IsRunningFromInstalledLocation()
    {
        var baseDir = AppContext.BaseDirectory;
        var programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        var programFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);

        return (!string.IsNullOrEmpty(programFiles) && baseDir.StartsWith(programFiles, StringComparison.OrdinalIgnoreCase))
            || (!string.IsNullOrEmpty(programFilesX86) && baseDir.StartsWith(programFilesX86, StringComparison.OrdinalIgnoreCase));
    }

    private async Task<string> DownloadToTempFileAsync(string url, string fileName, CancellationToken cancellationToken)
    {
        var tempDir = Path.Combine(Path.GetTempPath(), "WinKitUpdate");
        Directory.CreateDirectory(tempDir);
        var tempPath = Path.Combine(tempDir, fileName);

        using var response = await _httpClient.GetAsync(url, HttpCompletionOption.ResponseHeadersRead, cancellationToken);
        response.EnsureSuccessStatusCode();

        await using (var fileStream = File.Create(tempPath))
        {
            await response.Content.CopyToAsync(fileStream, cancellationToken);
        }

        return tempPath;
    }

    private static async Task<bool> VerifyChecksumAsync(string filePath, string expectedHash, CancellationToken cancellationToken)
    {
        await using var stream = File.OpenRead(filePath);
        var actualHashBytes = await SHA256.HashDataAsync(stream, cancellationToken);
        var actualHash = Convert.ToHexString(actualHashBytes);

        return string.Equals(expectedHash, actualHash, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>GitHub's asset digest arrives as "sha256:&lt;hex&gt;"; the hex part is what we compare against.</summary>
    private static string? ParseSha256Digest(string? digest)
    {
        if (digest is null)
        {
            return null;
        }

        const string prefix = "sha256:";
        return digest.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) ? digest[prefix.Length..] : null;
    }

    private static void TryDelete(string path)
    {
        try
        {
            File.Delete(path);
        }
        catch (IOException)
        {
            // Best-effort cleanup.
        }
    }

    private static GitHubAsset? FindAsset(List<GitHubAsset> assets, Func<string, bool> nameMatches) =>
        assets.FirstOrDefault(a => a.Name is not null && nameMatches(a.Name));

    private static Version GetCurrentVersion() =>
        Assembly.GetEntryAssembly()?.GetName().Version ?? new Version(0, 0, 0);

    private static string GetCurrentVersionText() => GetCurrentVersion().ToString(3);

    private static UpdateCheckResult Failed(string message) =>
        new() { Status = UpdateCheckStatus.Failed, ErrorMessage = message };

    private static UpdateApplyResult FailedApply(string message) =>
        new() { Status = UpdateApplyStatus.Failed, ErrorMessage = message };

    private sealed class GitHubRelease
    {
        [JsonPropertyName("tag_name")]
        public string? TagName { get; set; }

        [JsonPropertyName("html_url")]
        public string? HtmlUrl { get; set; }

        [JsonPropertyName("body")]
        public string? Body { get; set; }

        [JsonPropertyName("assets")]
        public List<GitHubAsset>? Assets { get; set; }
    }

    private sealed class GitHubAsset
    {
        [JsonPropertyName("name")]
        public string? Name { get; set; }

        [JsonPropertyName("browser_download_url")]
        public string? BrowserDownloadUrl { get; set; }

        /// <summary>GitHub-computed digest, formatted "sha256:&lt;hex&gt;".</summary>
        [JsonPropertyName("digest")]
        public string? Digest { get; set; }
    }
}
