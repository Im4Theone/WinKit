using WinKit.Core.Models;

namespace WinKit.Core.Abstractions;

public interface INotificationService
{
    event EventHandler<NotificationRequest>? Notified;

    void Show(NotificationRequest request);
}
