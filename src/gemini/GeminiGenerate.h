#pragma once
#include <string>
#include <vector>
class PiiMasker;

struct GeminiGenerateResult
{
    bool success;
    unsigned long httpStatus;
    std::wstring text;
    std::wstring error;
    bool cancelled = false;
    bool retryable = false;
    std::wstring modelUsed;
    std::wstring notice;
};

GeminiGenerateResult GenerateMailSummary(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& thinkingLevel,
    const std::wstring& subject,
    const std::wstring& body,
    PiiMasker* masker = nullptr);

struct GeminiChatTurn
{
    bool isModel;
    std::wstring text;
};

GeminiGenerateResult GenerateMailConsultation(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& thinkingLevel,
    const std::wstring& subject,
    const std::wstring& body,
    const std::vector<GeminiChatTurn>& history,
    const std::wstring& question,
    PiiMasker* masker = nullptr);
