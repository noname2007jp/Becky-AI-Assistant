#pragma once

#include "GeminiGenerate.h"
#include "GeminiModels.h"
#include <algorithm>
#include <atomic>
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
    const std::function<GeminiGenerateResult(const std::wstring&, const std::wstring&)>& generate,
    const std::atomic<int>* cancelState = nullptr)
{
    GeminiGenerateResult result = generate(selectedModelId, selectedThinking);
    result.modelUsed = selectedModelId;
    if (!enabled || result.success || !result.retryable ||
        (cancelState && cancelState->load(std::memory_order_acquire) != 0))
        return result;

    // Only fall back to models explicitly identified as having a free tier by the latest
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
    {
        result.error += L"\n無料枠ありと料金確認済みのFlash代替モデルがありません。設定画面でモデル一覧を更新してください。";
        return result;
    }

    const GeminiModelOption& fallback = *candidates.front();
    if (cancelState && cancelState->load(std::memory_order_acquire) != 0)
        return result;
    GeminiGenerateResult alternate = generate(fallback.id, fallback.thinking);
    alternate.modelUsed = fallback.id;
    if (alternate.success)
    {
        alternate.notice = L"選択モデルの応答が不安定なため、別モデル「" +
            fallback.label + L"」に切り替えて生成しました。";
        return alternate;
    }

    alternate.error += L"\n最初のモデルのエラー: " + result.error;
    if (alternate.retryable)
        alternate.error += L"\n別モデル「" + fallback.label +
            L"」への切り替えも試しましたが、応答を取得できませんでした。";
    else
        alternate.error += L"\n別モデル「" + fallback.label +
            L"」へ切り替えましたが、こちらでもエラーが発生しました。";
    return alternate;
}
