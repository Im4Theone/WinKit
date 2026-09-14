namespace WinKit.Core.Abstractions;

/// <summary>Registers/unregisters WinKit itself in the current user's Windows startup entries.</summary>
public interface IAutoStartService
{
    bool IsEnabled();

    void SetEnabled(bool enabled);
}
