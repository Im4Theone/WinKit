using System.Windows;
using System.Windows.Controls;
using WinKit.UI.ViewModels;

namespace WinKit.UI.Views;

public partial class NotificationHostView : UserControl
{
    public NotificationHostView()
    {
        InitializeComponent();
    }

    private void Card_MouseEnter(object sender, RoutedEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: NotificationItemViewModel item })
        {
            item.Pause();
        }
    }

    private void Card_MouseLeave(object sender, RoutedEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: NotificationItemViewModel item })
        {
            item.Resume();
        }
    }
}
