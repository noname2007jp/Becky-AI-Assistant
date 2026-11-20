#pragma once
#include "gemini/GeminiGenerate.h"
#include "settings/AIProviderSettings.h"
#include "WorkersAIClient.h"
#include <atomic>
#include <string>
#include <vector>

class PiiMasker;

GeminiGenerateResult GenerateWorkersAIMailConsultation(
    const std::wstring& accountId,
    const std::wstring& apiToken,
    const std::wstring& modelId,
    const std::wstring& subject,
    const std::wstring& body,
    const std::vector<GeminiChatTurn>& history,
    const std::wstring& question,
    PiiMasker* masker = nullptr,
    const std::atomic<int>* cancelState = nullptr,
    const WorkersAIProgressCallback& progress = {},
    const AIProviderSettings* providerSettings = nullptr);
