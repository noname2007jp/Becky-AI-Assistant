#pragma once
#include <cstddef>
#include <string>
void LogGeminiRequest(const std::wstring& operation, const std::wstring& modelId,
    bool maskingEnabled, std::size_t maskedTermCount, const std::string& payload);
void LogGeminiResponse(const std::wstring& operation, const std::wstring& modelId,
    unsigned long httpStatus, bool success, const std::wstring& error);

void LogProviderEvent(const std::wstring& provider, const std::wstring& operation,
    const std::wstring& modelId, const std::wstring& phase, unsigned long httpStatus,
    bool success, unsigned long long elapsedMs, const std::wstring& error);
