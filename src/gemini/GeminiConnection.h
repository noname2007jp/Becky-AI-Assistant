#pragma once
#include <string>

enum class GeminiConnectionStatus
{
    Success,
    MissingApiKey,
    InvalidApiKey,
    PermissionDenied,
    RateLimited,
    ServerError,
    NetworkError,
    UnexpectedResponse
};

struct GeminiConnectionResult
{
    GeminiConnectionStatus status;
    unsigned long httpStatus;
    std::wstring message;
};

GeminiConnectionResult TestGeminiConnection(const std::wstring& apiKey);
