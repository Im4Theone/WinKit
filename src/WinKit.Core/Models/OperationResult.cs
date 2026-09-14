namespace WinKit.Core.Models;

/// <summary>
/// Result of a service-layer operation. Carries a user-safe message plus
/// optional technical detail so the UI can show a friendly error while
/// still allowing the user to copy diagnostic details.
/// </summary>
public sealed class OperationResult<T>
{
    public bool Success { get; private init; }
    public T? Value { get; private init; }
    public string? UserMessage { get; private init; }
    public string? TechnicalDetail { get; private init; }

    public static OperationResult<T> Ok(T value, string? message = null) => new()
    {
        Success = true,
        Value = value,
        UserMessage = message
    };

    public static OperationResult<T> Fail(string userMessage, string? technicalDetail = null) => new()
    {
        Success = false,
        UserMessage = userMessage,
        TechnicalDetail = technicalDetail
    };
}

public sealed class OperationResult
{
    public bool Success { get; private init; }
    public string? UserMessage { get; private init; }
    public string? TechnicalDetail { get; private init; }

    public static OperationResult Ok(string? message = null) => new() { Success = true, UserMessage = message };

    public static OperationResult Fail(string userMessage, string? technicalDetail = null) => new()
    {
        Success = false,
        UserMessage = userMessage,
        TechnicalDetail = technicalDetail
    };
}
