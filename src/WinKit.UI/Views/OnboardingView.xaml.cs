using System.Windows.Controls;

namespace WinKit.UI.Views;

public partial class OnboardingView : UserControl
{
    public OnboardingView()
    {
        InitializeComponent();
        Loaded += (_, _) => NameBox.Focus();
    }
}
