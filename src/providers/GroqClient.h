#pragma once

#include "gemini/GeminiGenerate.h"
#include "settings/AIProviderSettings.h"
#include <atomic>
#include <functional>
#include <string>

using GroqProgressCallback = std::function<void(const std::wstring&)>;

// Sends a chat-completions request to Groq's OpenAI-compatible API.
// Prompts must already be masked by the caller.
GeminiGenerateResult GenerateGroqChatCompletion(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& systemPrompt,
    const std::wstring& userPrompt,
    unsigned int maxOutputTokens,
    const std::atomic<int>* cancelState = nullptr,
    const GroqProgressCallback& progress = {},
    const AIProviderSettings* providerSettings = nullptr);
