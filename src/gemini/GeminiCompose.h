#pragma once
#include "GeminiGenerate.h"

GeminiGenerateResult GenerateNewMailDraft(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& thinkingLevel,
    const std::wstring& toneInstruction,
    const std::wstring& purpose,
    const std::wstring& requirements);
