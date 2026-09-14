using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;

namespace WinKit.UI.Behaviors;

/// <summary>
/// Click-to-sort for ListView/GridView columns. Usage: set
/// <c>behaviors:GridViewSort.AutoSort="True"</c> on the ListView and
/// <c>behaviors:GridViewSort.PropertyName="SomeProperty"</c> (dotted paths like
/// "App.Name" are fine) on each sortable GridViewColumn. Clicking a header toggles
/// ascending/descending and appends a small arrow to that column's header text.
/// </summary>
public static class GridViewSort
{
    public static readonly DependencyProperty PropertyNameProperty =
        DependencyProperty.RegisterAttached("PropertyName", typeof(string), typeof(GridViewSort));

    public static void SetPropertyName(DependencyObject element, string value) => element.SetValue(PropertyNameProperty, value);
    public static string? GetPropertyName(DependencyObject element) => (string?)element.GetValue(PropertyNameProperty);

    public static readonly DependencyProperty AutoSortProperty =
        DependencyProperty.RegisterAttached("AutoSort", typeof(bool), typeof(GridViewSort),
            new UIPropertyMetadata(false, OnAutoSortChanged));

    public static bool GetAutoSort(DependencyObject obj) => (bool)obj.GetValue(AutoSortProperty);
    public static void SetAutoSort(DependencyObject obj, bool value) => obj.SetValue(AutoSortProperty, value);

    private static readonly DependencyProperty SortedColumnHeaderProperty =
        DependencyProperty.RegisterAttached("SortedColumnHeader", typeof(GridViewColumnHeader), typeof(GridViewSort));

    private static readonly DependencyProperty OriginalHeaderTextProperty =
        DependencyProperty.RegisterAttached("OriginalHeaderText", typeof(string), typeof(GridViewSort));

    private static void OnAutoSortChanged(DependencyObject o, DependencyPropertyChangedEventArgs e)
    {
        if (o is not ListView listView)
        {
            return;
        }

        if ((bool)e.NewValue)
        {
            listView.AddHandler(GridViewColumnHeader.ClickEvent, new RoutedEventHandler(ColumnHeaderClick));
        }
        else
        {
            listView.RemoveHandler(GridViewColumnHeader.ClickEvent, new RoutedEventHandler(ColumnHeaderClick));
        }
    }

    private static void ColumnHeaderClick(object sender, RoutedEventArgs e)
    {
        if (e.OriginalSource is not GridViewColumnHeader { Column: not null } headerClicked)
        {
            return;
        }

        var propertyName = GetPropertyName(headerClicked.Column);
        if (string.IsNullOrEmpty(propertyName))
        {
            return;
        }

        if (FindAncestorListView(headerClicked) is not { } listView)
        {
            return;
        }

        var direction = ListSortDirection.Ascending;
        if (listView.GetValue(SortedColumnHeaderProperty) is GridViewColumnHeader previousHeader)
        {
            if (previousHeader == headerClicked && listView.Items.SortDescriptions.Count > 0 &&
                listView.Items.SortDescriptions[0].Direction == ListSortDirection.Ascending)
            {
                direction = ListSortDirection.Descending;
            }

            ResetHeaderText(previousHeader);
        }

        listView.Items.SortDescriptions.Clear();
        listView.Items.SortDescriptions.Add(new SortDescription(propertyName, direction));
        listView.SetValue(SortedColumnHeaderProperty, headerClicked);

        ApplyHeaderText(headerClicked, direction);
    }

    private static void ApplyHeaderText(GridViewColumnHeader header, ListSortDirection direction)
    {
        if (header.GetValue(OriginalHeaderTextProperty) is not string original)
        {
            original = header.Content?.ToString() ?? string.Empty;
            header.SetValue(OriginalHeaderTextProperty, original);
        }

        if (string.IsNullOrEmpty(original))
        {
            return;
        }

        header.Content = direction == ListSortDirection.Ascending ? $"{original} ↑" : $"{original} ↓";
    }

    private static void ResetHeaderText(GridViewColumnHeader header)
    {
        if (header.GetValue(OriginalHeaderTextProperty) is string original)
        {
            header.Content = original;
        }
    }

    private static ListView? FindAncestorListView(DependencyObject element)
    {
        var current = element;
        while (current is not null)
        {
            if (current is ListView listView)
            {
                return listView;
            }

            current = System.Windows.Media.VisualTreeHelper.GetParent(current);
        }

        return null;
    }
}
