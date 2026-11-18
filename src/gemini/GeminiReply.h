#pragma once
#include "GeminiGenerate.h"

std::wstring BuildMailReplyContent(
    const std::wstring& subject,
    const std::wstring& body,
    const std::wstring& consultationTranscript = L"",
    const std::wstring& replyRequirements = L"");

std::wstring BuildMailReplyPrompt(
    const std::wstring& toneInstruction,
    const std::wstring& subject,
    const std::wstring& body,
    const std::wstring& consultationTranscript = L"",
    const std::wstring& replyRequirements = L"");

GeminiGenerateResult GenerateMailReplyDraft(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& thinkingLevel,
    const std::wstring& toneInstruction,
    const std::wstring& subject,
    const std::wstring& body,
    const std::wstring& consultationTranscript = L"",
    PiiMasker* masker = nullptr,
    const std::wstring& replyRequirements = L"");
