#pragma once
#include "gemini/GeminiGenerate.h"
#include "settings/AIProviderSettings.h"
#include "GroqClient.h"
#include <atomic>
#include <string>
#include <vector>

class PiiMasker;

GeminiGenerateResult GenerateGroqMailConsultation(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& subject,
    const std::wstring& body,
    const std::vector<GeminiChatTurn>& history,
    const std::wstring& question,
    PiiMasker* masker = nullptr,
    const std::atomic<int>* cancelState = nullptr,
    const GroqProgressCallback& progress = {},
    const AIProviderSettings* providerSettings = nullptr);
