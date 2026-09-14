using System.ServiceProcess;
using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public sealed class ServicesManagerService : IServicesManagerService
{
    public Task<IReadOnlyList<ServiceEntry>> GetServicesAsync(CancellationToken cancellationToken = default)
    {
        return Task.Run<IReadOnlyList<ServiceEntry>>(() =>
        {
            return ServiceController.GetServices()
                .Select(s =>
                {
                    string startupType;
                    try
                    {
                        startupType = s.StartType.ToString();
                    }
                    catch (InvalidOperationException)
                    {
                        startupType = "Unknown";
                    }

                    return new ServiceEntry
                    {
                        Name = s.ServiceName,
                        DisplayName = s.DisplayName,
                        Status = s.Status.ToString(),
                        StartupType = startupType,
                        CanStart = s.Status == ServiceControllerStatus.Stopped,
                        CanStop = s.Status == ServiceControllerStatus.Running && s.CanStop
                    };
                })
                .OrderBy(s => s.DisplayName, StringComparer.OrdinalIgnoreCase)
                .ToList();
        }, cancellationToken);
    }

    public Task<OperationResult> StartAsync(string serviceName, CancellationToken cancellationToken = default) =>
        ChangeStateAsync(serviceName, controller => controller.Start(),
            ServiceControllerStatus.Running, "start", cancellationToken);

    public Task<OperationResult> StopAsync(string serviceName, CancellationToken cancellationToken = default) =>
        ChangeStateAsync(serviceName, controller => controller.Stop(),
            ServiceControllerStatus.Stopped, "stop", cancellationToken);

    private static Task<OperationResult> ChangeStateAsync(
        string serviceName,
        Action<ServiceController> action,
        ServiceControllerStatus targetStatus,
        string verb,
        CancellationToken cancellationToken)
    {
        return Task.Run(() =>
        {
            try
            {
                using var controller = new ServiceController(serviceName);
                action(controller);
                controller.WaitForStatus(targetStatus, TimeSpan.FromSeconds(10));
                return OperationResult.Ok();
            }
            catch (InvalidOperationException ex)
            {
                return OperationResult.Fail(
                    $"Unable to {verb} \"{serviceName}\". This may require administrator access.", ex.Message);
            }
            catch (System.ComponentModel.Win32Exception ex)
            {
                return OperationResult.Fail(
                    $"Unable to {verb} \"{serviceName}\". This may require administrator access.", ex.Message);
            }
            catch (System.ServiceProcess.TimeoutException ex)
            {
                return OperationResult.Fail($"Timed out waiting for \"{serviceName}\" to {verb}.", ex.Message);
            }
        }, cancellationToken);
    }
}
