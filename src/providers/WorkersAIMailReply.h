#pragma once

#include "gemini/GeminiGenerate.h"

#include <atomic>
#include <string>

class PiiMasker;

// Creates a Japanese business-email reply draft through Cloudflare Workers AI.
// The caller owns the masker and has already shown the masked preview.
GeminiGenerateResult GenerateWorkersAIMailReplyDraft(
    const std::wstring& accountId,
    const std::wstring& apiToken,
    const std::wstring& modelId,
    const std::wstring& toneInstruction,
    const std::wstring& subject,
    const std::wstring& body,
    const std::wstring& replyRequirements,
    PiiMasker* masker,
    const std::atomic<int>* cancelState = nullptr);
