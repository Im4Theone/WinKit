using WinKit.Core.Abstractions;
using WinKit.Core.Models;

namespace WinKit.Infrastructure;

public sealed class NotificationService : INotificationService
{
    public event EventHandler<NotificationRequest>? Notified;

    public void Show(NotificationRequest request) => Notified?.Invoke(this, request);
}
