using System.Collections.ObjectModel;
using WinKit.Core.Models;

namespace WinKit.Core.Abstractions;

public interface IActivityLogService
{
    ReadOnlyObservableCollection<ActivityEntry> Entries { get; }

    void Record(string title, ActivityKind kind = ActivityKind.Info, string? detail = null);
}
