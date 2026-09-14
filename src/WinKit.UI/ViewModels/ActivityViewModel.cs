using System.Collections.ObjectModel;
using System.Collections.Specialized;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.Core.Abstractions;
using WinKit.Core.Models;

namespace WinKit.UI.ViewModels;

public enum ActivityFilter
{
    All,
    Actions,
    ThemesAndSettings
}

public sealed partial class ActivityViewModel : ObservableObject
{
    private readonly ReadOnlyObservableCollection<ActivityEntry> _allEntries;

    public ActivityViewModel(IActivityLogService activityLogService)
    {
        _allEntries = activityLogService.Entries;
        ((INotifyCollectionChanged)_allEntries).CollectionChanged += (_, _) => RefreshFilteredEntries();
        RefreshFilteredEntries();
    }

    public ObservableCollection<ActivityEntry> FilteredEntries { get; } = new();

    [ObservableProperty]
    private ActivityFilter _selectedFilter = ActivityFilter.All;

    public bool IsAllSelected => SelectedFilter == ActivityFilter.All;
    public bool IsActionsSelected => SelectedFilter == ActivityFilter.Actions;
    public bool IsThemesAndSettingsSelected => SelectedFilter == ActivityFilter.ThemesAndSettings;

    partial void OnSelectedFilterChanged(ActivityFilter value)
    {
        OnPropertyChanged(nameof(IsAllSelected));
        OnPropertyChanged(nameof(IsActionsSelected));
        OnPropertyChanged(nameof(IsThemesAndSettingsSelected));
        RefreshFilteredEntries();
    }

    [RelayCommand]
    private void ShowAll() => SelectedFilter = ActivityFilter.All;

    [RelayCommand]
    private void ShowActions() => SelectedFilter = ActivityFilter.Actions;

    [RelayCommand]
    private void ShowThemesAndSettings() => SelectedFilter = ActivityFilter.ThemesAndSettings;

    private void RefreshFilteredEntries()
    {
        IEnumerable<ActivityEntry> source = SelectedFilter switch
        {
            ActivityFilter.Actions => _allEntries.Where(e => e.Category == ActivityCategory.Action),
            ActivityFilter.ThemesAndSettings => _allEntries.Where(e => e.Category == ActivityCategory.ThemeAndSettings),
            _ => _allEntries
        };

        FilteredEntries.Clear();
        foreach (var entry in source)
        {
            FilteredEntries.Add(entry);
        }
    }
}
