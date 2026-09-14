using System.Collections.ObjectModel;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using WinKit.Core.Abstractions;

namespace WinKit.UI.ViewModels;

public sealed partial class NotificationHostViewModel : ObservableObject
{
    private readonly Dispatcher _dispatcher;

    public NotificationHostViewModel(INotificationService notificationService)
    {
        _dispatcher = Dispatcher.CurrentDispatcher;

        notificationService.Notified += (_, request) => _dispatcher.Invoke(() =>
        {
            Items.Insert(0, new NotificationItemViewModel(request, Remove));
        });
    }

    public ObservableCollection<NotificationItemViewModel> Items { get; } = new();

    private void Remove(NotificationItemViewModel item) => Items.Remove(item);
}
