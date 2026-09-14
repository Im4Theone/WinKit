using WinKit.Core.Models;
using WinKit.SystemTools.Models;

namespace WinKit.SystemTools.Services;

public interface IEnvironmentVariablesService
{
    IReadOnlyList<EnvironmentVariableEntry> GetVariables();

    OperationResult SetVariable(EnvironmentVariableScope scope, string name, string value);

    OperationResult DeleteVariable(EnvironmentVariableScope scope, string name);
}
