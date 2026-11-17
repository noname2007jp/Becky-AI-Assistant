#pragma once
#include "GeminiGenerate.h"

std::wstring BuildEmailProofreadInstruction(int level);

GeminiGenerateResult GenerateEmailProofread(
    const std::wstring& apiKey, const std::wstring& modelId,
    const std::wstring& thinkingLevel, const std::wstring& body,
    int level, PiiMasker* masker = nullptr);
