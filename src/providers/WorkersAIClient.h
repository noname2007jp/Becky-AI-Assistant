#pragma once
#include "gemini/GeminiGenerate.h"
#include "settings/AIProviderSettings.h"
#include <atomic>
#include <functional>
#include <string>

using WorkersAIProgressCallback = std::function<void(const std::wstring&)>;

GeminiGenerateResult GenerateWorkersAIChatCompletion(
    const std::wstring& apiToken,
    const std::wstring& accountId,
    const std::wstring& modelId,
    const std::wstring& systemPrompt,
    const std::wstring& userPrompt,
    unsigned int maxOutputTokens,
    const std::atomic<int>* cancelState = nullptr,
    const WorkersAIProgressCallback& progress = {},
    const AIProviderSettings* providerSettings = nullptr);
