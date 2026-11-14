#pragma once
#include "GeminiGenerate.h"

GeminiGenerateResult GenerateMailReplyDraft(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& thinkingLevel,
    const std::wstring& toneInstruction,
    const std::wstring& subject,
    const std::wstring& from,
    const std::wstring& body,
    const std::wstring& consultationTranscript = L"");
