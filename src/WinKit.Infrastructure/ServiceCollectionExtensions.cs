using Microsoft.Extensions.DependencyInjection;
using WinKit.Core.Abstractions;

namespace WinKit.Infrastructure;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddWinKitInfrastructure(this IServiceCollection services)
    {
        services.AddSingleton<IUserProfileService, UserProfileService>();
        services.AddSingleton<IActivityLogService, ActivityLogService>();
        services.AddSingleton<INotificationService, NotificationService>();
        services.AddSingleton<IElevationService, ElevationService>();
        services.AddSingleton<IAppSettingsService, AppSettingsService>();
        services.AddSingleton<IAutoStartService, AutoStartService>();
        return services;
    }
}
