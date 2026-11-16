#pragma once

#include "GeminiGenerate.h"
#include "GeminiModels.h"
#include <algorithm>
#include <functional>
#include <vector>

inline bool IsRetryableGeminiStatus(unsigned long status)
{
    return status == 408 || status == 429 || status == 500 ||
        status == 502 || status == 503 || status == 504;
}

inline GeminiGenerateResult GenerateWithModelFallback(
    const std::wstring& selectedModelId,
    const std::wstring& selectedThinking,
    const std::vector<GeminiModelOption>& models,
    bool enabled,
    const std::function<GeminiGenerateResult(const std::wstring&, const std::wstring&)>& generate)
{
    GeminiGenerateResult result = generate(selectedModelId, selectedThinking);
    result.modelUsed = selectedModelId;
    if (!enabled || result.success || !result.retryable)
        return result;

    // Only fall back to models explicitly identified as free by the latest
    // official pricing refresh. Never silently switch to an unknown/paid model.
    std::vector<const GeminiModelOption*> candidates;
    for (const auto& model : models)
    {
        if (model.id == selectedModelId || !model.freeTier || !model.pricingKnown)
            continue;
        if (model.id.find(L"flash") == std::wstring::npos)
            continue;
        candidates.push_back(&model);
    }
    std::stable_sort(candidates.begin(), candidates.end(),
        [](const GeminiModelOption* a, const GeminiModelOption* b) {
            const bool aLite = a->id.find(L"flash-lite") != std::wstring::npos;
            const bool bLite = b->id.find(L"flash-lite") != std::wstring::npos;
            return aLite && !bLite;
        });

    if (candidates.empty())
        return result;

    const GeminiModelOption& fallback = *candidates.front();
    GeminiGenerateResult alternate = generate(fallback.id, fallback.thinking);
    alternate.modelUsed = fallback.id;
    if (alternate.success)
    {
        alternate.notice = L"選択モデルの応答が不安定なため、別モデル「" +
            fallback.label + L"」に切り替えて生成しました。";
        return alternate;
    }

    alternate.error += L"\n別モデル「" + fallback.label +
        L"」への切り替えも試しましたが、応答を取得できませんでした。";
    return alternate;
}
