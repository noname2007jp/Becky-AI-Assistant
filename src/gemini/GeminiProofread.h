#pragma once
#include "GeminiGenerate.h"

GeminiGenerateResult GenerateEmailProofread(
    const std::wstring& apiKey, const std::wstring& modelId,
    const std::wstring& thinkingLevel, const std::wstring& body,
    PiiMasker* masker = nullptr);
