using Microsoft.Extensions.DependencyInjection;

namespace WinKit.Themes;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddWinKitThemes(this IServiceCollection services)
    {
        services.AddSingleton<IThemeService, ThemeService>();
        return services;
    }
}
