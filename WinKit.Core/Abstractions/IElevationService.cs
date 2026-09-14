namespace WinKit.Core.Abstractions;

public interface IElevationService
{
    bool IsElevated { get; }

    /// <summary>
    /// Relaunches the current executable with elevation, passing the given
    /// arguments. Returns false if the user declined the UAC prompt.
    /// </summary>
    bool RelaunchElevated(string arguments);
}
