using System.Collections.ObjectModel;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;

namespace WinKit.Infrastructure;

public sealed class ActivityLogService : IActivityLogService
{
    private const int MaxEntries = 50;

    private readonly ObservableCollection<ActivityEntry> _entries = new();

    public ActivityLogService()
    {
        Entries = new ReadOnlyObservableCollection<ActivityEntry>(_entries);
    }

    public ReadOnlyObservableCollection<ActivityEntry> Entries { get; }

    public void Record(string title, ActivityKind kind = ActivityKind.Info, string? detail = null)
    {
        _entries.Insert(0, new ActivityEntry { Title = title, Kind = kind, Detail = detail });
        while (_entries.Count > MaxEntries)
        {
            _entries.RemoveAt(_entries.Count - 1);
        }
    }
}
