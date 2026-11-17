#pragma once
#include "GeminiGenerate.h"

std::wstring BuildEmailProofreadPrompt(const std::wstring& body);

GeminiGenerateResult GenerateEmailProofread(
    const std::wstring& apiKey, const std::wstring& modelId,
    const std::wstring& thinkingLevel, const std::wstring& body,
    PiiMasker* masker = nullptr);
