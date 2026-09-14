using WinKit.UI.Navigation;
using WinKit.UI.ViewModels.SystemTools;

namespace WinKit.UI.ViewModels;

public sealed class SystemToolsViewModel : ToolPageViewModelBase, IAsyncInitializable
{
    public SystemToolsViewModel(
        ProcessManagerViewModel processManager,
        StartupManagerViewModel startupManager,
        ServicesManagerViewModel servicesManager,
        InstalledAppsViewModel installedApps,
        EnvironmentVariablesViewModel environmentVariables,
        HostsFileViewModel hostsFile,
        SystemInformationViewModel systemInformation)
        : base(new ToolTabItem[]
        {
            new("Process Manager", processManager, () => processManager.RefreshCommand.ExecuteAsync(null)),
            new("Startup Manager", startupManager, () => startupManager.RefreshCommand.ExecuteAsync(null)),
            new("Services", servicesManager, () => servicesManager.RefreshCommand.ExecuteAsync(null)),
            new("Installed Applications", installedApps, () => installedApps.RefreshCommand.ExecuteAsync(null)),
            new("Environment Variables", environmentVariables, () => environmentVariables.RefreshCommand.ExecuteAsync(null)),
            new("Hosts File", hostsFile, () => hostsFile.LoadCommand.ExecuteAsync(null)),
            new("System Information", systemInformation, () => systemInformation.RefreshCommand.ExecuteAsync(null)),
        })
    {
    }

    public Task InitializeAsync()
    {
        SelectFirstTab();
        return Task.CompletedTask;
    }
}
