using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Core.Models;
using WinKit.Themes;
using WinKit.UI.Theming;

namespace WinKit.UI.ViewModels;

public sealed partial class NotificationItemViewModel : ObservableObject
{
    private readonly Action? _action;
    private readonly Action<NotificationItemViewModel> _onDismiss;
    private readonly DispatcherTimer? _timer;
    private DateTime _timerStartedAt;
    private TimeSpan _remaining;

    public NotificationItemViewModel(NotificationRequest request, Action<NotificationItemViewModel> onDismiss)
    {
        Title = request.Title;
        Description = request.Description;
        Severity = request.Severity;
        ActionText = request.ActionText;
        _action = request.Action;
        _onDismiss = onDismiss;

        // Read once at creation rather than tracking live theme changes — a toast
        // is short-lived, so it doesn't need to react to a mid-life intensity edit.
        AnimationsEnabled = AnimationProfile.CurrentIntensity != AnimationIntensity.Off;

        if (request.AutoDismissAfter is { } delay)
        {
            _remaining = delay;
            _timer = new DispatcherTimer();
            _timer.Tick += (_, _) =>
            {
                _timer!.Stop();
                DismissCommand.Execute(null);
            };
            StartTimer();
        }
    }

    public string Title { get; }
    public string? Description { get; }
    public NotificationSeverity Severity { get; }
    public string? ActionText { get; }
    public bool AnimationsEnabled { get; }

    [ObservableProperty]
    private bool _isVisible = true;

    /// <summary>Pauses the auto-dismiss countdown, preserving the remaining time (called while the pointer hovers the card).</summary>
    public void Pause()
    {
        if (_timer is not { IsEnabled: true })
        {
            return;
        }

        _timer.Stop();
        _remaining -= DateTime.UtcNow - _timerStartedAt;
        if (_remaining < TimeSpan.Zero)
        {
            _remaining = TimeSpan.Zero;
        }
    }

    /// <summary>Resumes the auto-dismiss countdown from wherever Pause left it.</summary>
    public void Resume()
    {
        if (_timer is null)
        {
            return;
        }

        if (_remaining <= TimeSpan.Zero)
        {
            DismissCommand.Execute(null);
            return;
        }

        StartTimer();
    }

    private void StartTimer()
    {
        if (_timer is null)
        {
            return;
        }

        _timerStartedAt = DateTime.UtcNow;
        _timer.Interval = _remaining;
        _timer.Start();
    }

    [RelayCommand]
    private async Task DismissAsync()
    {
        if (!IsVisible)
        {
            return;
        }

        _timer?.Stop();
        IsVisible = false;
        if (AnimationsEnabled)
        {
            await Task.Delay(160);
        }

        _onDismiss(this);
    }

    [RelayCommand]
    private async Task InvokeActionAsync()
    {
        _action?.Invoke();
        await DismissAsync();
    }
}
