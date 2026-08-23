using System.Diagnostics;

// args: [0] staging directory (extracted update), [1] WinKit install directory,
// [2] PID of the WinKit process to wait for. Launched by WinKit.Infrastructure's
// UpdateService right before WinKit exits; this process does the actual file
// replacement so it never happens from inside the running app.

if (args.Length < 3 || !int.TryParse(args[2], out var winKitPid))
{
    Log("Missing or invalid arguments; expected <stagingDir> <installDir> <pid>.");
    return 1;
}

var stagingDir = args[0];
var installDir = args[1];

try
{
    WaitForProcessExit(winKitPid, TimeSpan.FromSeconds(30));

    // Give the OS a brief moment to fully release file handles after the process exits.
    await Task.Delay(500);

    CopyDirectoryWithRetry(stagingDir, installDir);

    TryDeleteDirectory(stagingDir);

    RelaunchWinKit(installDir);
    return 0;
}
catch (Exception ex)
{
    Log($"Update failed: {ex}");

    // Best-effort: get the user back into a working app even if the update itself failed.
    try
    {
        RelaunchWinKit(installDir);
    }
    catch (Exception relaunchEx)
    {
        Log($"Relaunch after failed update also failed: {relaunchEx}");
    }

    return 1;
}

static void WaitForProcessExit(int pid, TimeSpan timeout)
{
    try
    {
        var process = Process.GetProcessById(pid);
        process.WaitForExit((int)timeout.TotalMilliseconds);
    }
    catch (ArgumentException)
    {
        // Already exited.
    }
}

static void CopyDirectoryWithRetry(string sourceDir, string destDir)
{
    foreach (var file in Directory.GetFiles(sourceDir, "*", SearchOption.AllDirectories))
    {
        var relativePath = Path.GetRelativePath(sourceDir, file);
        var destPath = Path.Combine(destDir, relativePath);
        var destPathDir = Path.GetDirectoryName(destPath);
        if (!string.IsNullOrEmpty(destPathDir))
        {
            Directory.CreateDirectory(destPathDir);
        }

        CopyFileWithRetry(file, destPath);
    }
}

static void CopyFileWithRetry(string sourceFile, string destFile)
{
    const int maxAttempts = 5;
    for (var attempt = 1; attempt <= maxAttempts; attempt++)
    {
        try
        {
            File.Copy(sourceFile, destFile, overwrite: true);
            return;
        }
        catch (Exception ex) when ((ex is IOException or UnauthorizedAccessException) && attempt < maxAttempts)
        {
            // A file can still be briefly locked right after the parent process exits; retry.
            Thread.Sleep(300);
        }
    }
}

static void TryDeleteDirectory(string path)
{
    try
    {
        Directory.Delete(path, recursive: true);
    }
    catch (IOException)
    {
        // Best-effort cleanup; a leftover staging folder under %TEMP% is harmless.
    }
}

static void RelaunchWinKit(string installDir)
{
    var exePath = Path.Combine(installDir, "WinKit.exe");
    Process.Start(new ProcessStartInfo
    {
        FileName = exePath,
        WorkingDirectory = installDir,
        UseShellExecute = true
    });
}

static void Log(string message)
{
    try
    {
        var logDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "WinKit", "Logs");
        Directory.CreateDirectory(logDir);
        File.AppendAllText(
            Path.Combine(logDir, "crash.log"),
            $"{DateTime.Now:O}{Environment.NewLine}[WinKit.Updater] {message}{Environment.NewLine}{new string('-', 40)}{Environment.NewLine}");
    }
    catch (IOException)
    {
        // Best-effort logging.
    }
}
