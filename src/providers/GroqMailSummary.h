#pragma once

#include "gemini/GeminiGenerate.h"
#include "settings/AIProviderSettings.h"
#include <atomic>
#include <string>

class PiiMasker;

// Creates a Japanese business-email summary through Groq.
// The caller owns the masker and has already shown the masked preview.
GeminiGenerateResult GenerateGroqMailSummary(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& subject,
    const std::wstring& body,
    PiiMasker* masker,
    const std::atomic<int>* cancelState = nullptr,
    const AIProviderSettings* providerSettings = nullptr);
