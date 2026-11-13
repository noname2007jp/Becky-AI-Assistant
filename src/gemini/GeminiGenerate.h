#pragma once
#include <string>

struct GeminiGenerateResult
{
    bool success;
    unsigned long httpStatus;
    std::wstring text;
    std::wstring error;
};

GeminiGenerateResult GenerateMailSummary(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& thinkingLevel,
    const std::wstring& subject,
    const std::wstring& from,
    const std::wstring& body);
