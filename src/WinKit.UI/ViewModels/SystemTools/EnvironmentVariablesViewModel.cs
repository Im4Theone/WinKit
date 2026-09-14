using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinKit.SystemTools.Models;
using WinKit.SystemTools.Services;
using WinKit.UI.Dialogs;

namespace WinKit.UI.ViewModels.SystemTools;

public sealed partial class EnvironmentVariablesViewModel : ObservableObject
{
    private readonly IEnvironmentVariablesService _environmentVariablesService;
    private readonly IDialogService _dialogService;

    public EnvironmentVariablesViewModel(IEnvironmentVariablesService environmentVariablesService, IDialogService dialogService)
    {
        _environmentVariablesService = environmentVariablesService;
        _dialogService = dialogService;
    }

    public ObservableCollection<EnvironmentVariableEntry> Variables { get; } = new();

    public IReadOnlyList<EnvironmentVariableScope> Scopes { get; } = Enum.GetValues<EnvironmentVariableScope>();

    [ObservableProperty]
    private string _newName = string.Empty;

    [ObservableProperty]
    private string _newValue = string.Empty;

    [ObservableProperty]
    private EnvironmentVariableScope _newScope = EnvironmentVariableScope.User;

    [RelayCommand]
    private Task RefreshAsync()
    {
        Variables.Clear();
        foreach (var variable in _environmentVariablesService.GetVariables())
        {
            Variables.Add(variable);
        }

        return Task.CompletedTask;
    }

    [RelayCommand]
    private async Task AddAsync()
    {
        if (string.IsNullOrWhiteSpace(NewName))
        {
            return;
        }

        var result = _environmentVariablesService.SetVariable(NewScope, NewName, NewValue);
        if (!result.Success)
        {
            _dialogService.ShowError("Couldn't set variable", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
            return;
        }

        NewName = string.Empty;
        NewValue = string.Empty;
        await RefreshAsync();
    }

    [RelayCommand]
    private async Task DeleteAsync(EnvironmentVariableEntry entry)
    {
        var confirmed = _dialogService.Confirm(new ConfirmationRequest
        {
            Title = "Delete environment variable",
            Message = $"Delete \"{entry.Name}\"? Applications that rely on it may stop working correctly.",
            ConfirmText = "Delete",
            IsDestructive = true
        });

        if (!confirmed)
        {
            return;
        }

        var result = _environmentVariablesService.DeleteVariable(entry.Scope, entry.Name);
        if (!result.Success)
        {
            _dialogService.ShowError("Couldn't delete variable", result.UserMessage ?? "Unknown error.", result.TechnicalDetail);
        }

        await RefreshAsync();
    }
}
