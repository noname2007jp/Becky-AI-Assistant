#pragma once
#include <cstddef>
#include <string>
void LogGeminiRequest(const std::wstring& operation, const std::wstring& modelId,
    bool maskingEnabled, std::size_t maskedTermCount, const std::string& payload);
void LogGeminiResponse(const std::wstring& operation, const std::wstring& modelId,
    unsigned long httpStatus, bool success, const std::wstring& error);
