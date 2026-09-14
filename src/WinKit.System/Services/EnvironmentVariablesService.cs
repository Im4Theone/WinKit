using System.Collections;
using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public sealed class EnvironmentVariablesService : IEnvironmentVariablesService
{
    public IReadOnlyList<EnvironmentVariableEntry> GetVariables()
    {
        var entries = new List<EnvironmentVariableEntry>();
        AddFrom(entries, EnvironmentVariableTarget.User, EnvironmentVariableScope.User);
        AddFrom(entries, EnvironmentVariableTarget.Machine, EnvironmentVariableScope.Machine);
        return entries.OrderBy(e => e.Scope).ThenBy(e => e.Name, StringComparer.OrdinalIgnoreCase).ToList();
    }

    public OperationResult SetVariable(EnvironmentVariableScope scope, string name, string value)
    {
        if (string.IsNullOrWhiteSpace(name))
        {
            return OperationResult.Fail("Variable name cannot be empty.");
        }

        try
        {
            Environment.SetEnvironmentVariable(name, value, ToTarget(scope));
            return OperationResult.Ok();
        }
        catch (System.Security.SecurityException ex)
        {
            return OperationResult.Fail(
                "Changing a system environment variable requires administrator access.", ex.Message);
        }
    }

    public OperationResult DeleteVariable(EnvironmentVariableScope scope, string name)
    {
        try
        {
            Environment.SetEnvironmentVariable(name, null, ToTarget(scope));
            return OperationResult.Ok();
        }
        catch (System.Security.SecurityException ex)
        {
            return OperationResult.Fail(
                "Changing a system environment variable requires administrator access.", ex.Message);
        }
    }

    private static void AddFrom(
        List<EnvironmentVariableEntry> entries, EnvironmentVariableTarget target, EnvironmentVariableScope scope)
    {
        var variables = Environment.GetEnvironmentVariables(target);
        foreach (DictionaryEntry entry in variables)
        {
            entries.Add(new EnvironmentVariableEntry
            {
                Name = (string)entry.Key,
                Value = entry.Value as string ?? string.Empty,
                Scope = scope
            });
        }
    }

    private static EnvironmentVariableTarget ToTarget(EnvironmentVariableScope scope) => scope switch
    {
        EnvironmentVariableScope.Machine => EnvironmentVariableTarget.Machine,
        _ => EnvironmentVariableTarget.User
    };
}
