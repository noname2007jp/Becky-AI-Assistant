#pragma once

#include "GeminiGenerate.h"
#include "GeminiModels.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <vector>
#include <utility>

inline bool IsRetryableGeminiStatus(unsigned long status)
{
    return status == 404 || status == 408 || status == 429 || (status >= 500 && status <= 599);
}

inline int GeminiFallbackPriority(const std::wstring& id)
{
    // Google currently recommends 3.8 Flash / 3.5 Flash-Lite for new projects.
    // 2.5 models may appear in models.list but are restricted to projects with prior usage.
    if (id.find(L"gemini-2.5-") == 0) return -1;
    if (id.find(L"gemini-3.8-flash") == 0) return 100;
    if (id.find(L"gemini-3.7-flash") == 0) return 95;
    if (id.find(L"gemini-3.6-flash") == 0) return 90;
    if (id.find(L"gemini-3.5-flash-lite") == 0) return 85;
    if (id.find(L"gemini-3.5-flash") == 0) return 80;
    if (id.find(L"gemini-3.1-flash-lite") == 0) return 70;
    if (id.find(L"flash") != std::wstring::npos) return 10;
    return -1;
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
    const std::wstring initialError = result.error;
    if (!enabled || result.success || !result.retryable ||
        (cancelState && cancelState->load(std::memory_order_acquire) != 0))
        return result;

    // Only use models explicitly marked as free-tier by the latest official pricing refresh.
    // Exclude 2.5: models.list may expose it even when this project is not eligible to call it.
    std::vector<const GeminiModelOption*> candidates;
    for (const auto& model : models)
    {
        const int priority = GeminiFallbackPriority(model.id);
        if (model.id == selectedModelId || !model.freeTier || !model.pricingKnown ||
            priority < 0 || model.id.find(L"flash") == std::wstring::npos)
            continue;
        candidates.push_back(&model);
    }
    std::stable_sort(candidates.begin(), candidates.end(),
        [](const GeminiModelOption* a, const GeminiModelOption* b) {
            return GeminiFallbackPriority(a->id) > GeminiFallbackPriority(b->id);
        });

    if (candidates.empty())
    {
        result.error += L"\n利用可能性と無料枠を確認できた新しいFlash代替モデルがありません。設定画面でモデル一覧を更新してください。";
        return result;
    }

    std::wstring attempted;
    // Limit fallback attempts to two models to avoid long waits or excessive API calls.
    const size_t maxAttempts = (std::min)(candidates.size(), static_cast<size_t>(2));
    for (size_t i = 0; i < maxAttempts; ++i)
    {
        if (cancelState && cancelState->load(std::memory_order_acquire) != 0)
            return result;

        const GeminiModelOption& fallback = *candidates[i];
        GeminiGenerateResult alternate = generate(fallback.id, fallback.thinking);
        alternate.modelUsed = fallback.id;
        if (alternate.success)
        {
            alternate.notice = L"選択モデル「" + selectedModelId +
                L"」でエラーが発生したため、別モデル「" + fallback.label +
                L"」に切り替えて生成しました。";
            if (!attempted.empty())
                alternate.notice += L"（利用できなかった候補: " + attempted + L"）";
            return alternate;
        }

        if (!attempted.empty()) attempted += L"、";
        attempted += fallback.label;
        if (alternate.httpStatus == 200)
            attempted += L"（応答内容の検証に失敗: " + alternate.error + L"）";
        else if (alternate.httpStatus == 0)
            attempted += L"（通信エラー: " + alternate.error + L"）";
        else
            attempted += L"（HTTP " + std::to_wstring(alternate.httpStatus) + L": " + alternate.error + L"）";

        // 404 means this project cannot call that model even if models.list exposed it.
        // Transient failures can also justify trying the next free-tier candidate.
        if (alternate.httpStatus != 404 && !alternate.retryable)
        {
            alternate.error += L"\n最初のモデルのエラー: " + initialError +
                L"\n代替モデル「" + fallback.label + L"」でも解決できませんでした。";
            return alternate;
        }
        result = std::move(alternate);
    }

    result.error += L"\n最初のモデルのエラー: " + initialError;
    result.error += L"\n代替モデルを試しましたが、利用可能な応答を取得できませんでした: " + attempted;
    return result;
}
