#pragma once
#include "gemini/GeminiGenerate.h"
#include "settings/AIProviderSettings.h"
#include <atomic>
#include <string>
class PiiMasker;
GeminiGenerateResult GenerateGroqMailReplyDraft(
    const std::wstring& apiKey, const std::wstring& modelId,
    const std::wstring& toneInstruction, const std::wstring& subject,
    const std::wstring& body, const std::wstring& replyRequirements,
    PiiMasker* masker, const std::atomic<int>* cancelState = nullptr,
    const AIProviderSettings* providerSettings = nullptr);
