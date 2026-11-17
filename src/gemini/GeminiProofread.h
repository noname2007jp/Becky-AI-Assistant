#pragma once
#include "GeminiGenerate.h"

std::wstring BuildEmailProofreadInstruction(const std::wstring& body);

GeminiGenerateResult GenerateEmailProofread(
    const std::wstring& apiKey, const std::wstring& modelId,
    const std::wstring& thinkingLevel, const std::wstring& body,
    PiiMasker* masker = nullptr);
