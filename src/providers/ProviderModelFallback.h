#pragma once

#include "gemini/GeminiGenerate.h"
#include "ProviderModelCatalogStore.h"
#include "ProviderModelEligibility.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Tries a small number of other models from the last successfully downloaded
// provider catalog when the selected model is unavailable or the request fails
// transiently. Credentials and prompts are never sent to another provider.
template <typename Generate>
GeminiGenerateResult GenerateWithProviderModelFallback(
    int provider,
    const std::wstring& selectedModelId,
    const std::atomic<int>* cancelState,
    const std::function<void(const std::wstring&)>& progress,
    Generate generate)
{
    GeminiGenerateResult result = generate(selectedModelId);
    result.modelUsed = selectedModelId;
    if (result.success || result.cancelled ||
        (cancelState && cancelState->load(std::memory_order_acquire) != 0) ||
        !(result.retryable || result.httpStatus == 404))
        return result;

    std::vector<std::wstring> cachedModels;
    std::wstring catalogError;
    if (!LoadProviderModelCatalog(provider, cachedModels, catalogError) || cachedModels.empty())
    {
        result.notice = L"代替モデル一覧が保存されていないため、自動切り替えは行いませんでした。設定画面でモデル一覧を更新してください。";
        return result;
    }

    std::sort(cachedModels.begin(), cachedModels.end());
    cachedModels.erase(std::unique(cachedModels.begin(), cachedModels.end()), cachedModels.end());

    const std::wstring originalError = result.error;
    const unsigned long originalStatus = result.httpStatus;
    std::wstring attempted;
    unsigned int tried = 0;
    constexpr unsigned int kMaxFallbackModels = 3;

    for (const auto& candidate : cachedModels)
    {
        if (candidate == selectedModelId) continue;
        const bool eligible = provider == 1
            ? provider_model_eligibility::IsGroqChatCompletionModelEligible(candidate)
            : provider_model_eligibility::IsWorkersAIChatCompletionModelEligible(candidate);
        if (!eligible) continue;
        if (cancelState && cancelState->load(std::memory_order_acquire) != 0)
        {
            result.cancelled = true;
            result.text.clear();
            return result;
        }
        if (tried >= kMaxFallbackModels) break;

        ++tried;
        if (progress)
            progress(L"選択モデルでエラーが発生したため、代替モデルを試しています（" +
                std::to_wstring(tried) + L"/" + std::to_wstring(kMaxFallbackModels) + L"）");

        GeminiGenerateResult alternate = generate(candidate);
        alternate.modelUsed = candidate;
        if (alternate.success)
        {
            alternate.notice = L"選択モデル「" + selectedModelId +
                L"」でエラーが発生したため、別モデル「" + candidate +
                L"」に切り替えて生成しました。";
            if (!attempted.empty())
                alternate.notice += L" 利用できなかった候補: " + attempted;
            return alternate;
        }

        if (!attempted.empty()) attempted += L"、";
        attempted += candidate + L"（";
        attempted += alternate.httpStatus
            ? L"HTTP " + std::to_wstring(alternate.httpStatus)
            : L"通信エラー";
        attempted += L"）";

        // Authentication, permission and malformed-request errors are not
        // model-specific fallback conditions. Stop rather than hiding them.
        if (!(alternate.retryable || alternate.httpStatus == 404))
        {
            alternate.error += L"\n選択モデルのエラー: " + originalError;
            alternate.notice = L"代替モデルでも解決しないエラーが発生したため、自動切り替えを中止しました。";
            return alternate;
        }
        result = std::move(alternate);
    }

    result.success = false;
    result.text.clear();
    result.modelUsed = selectedModelId;
    result.error = L"選択モデルおよび代替モデルで応答を取得できませんでした。";
    result.error += L"\n選択モデル「" + selectedModelId + L"」";
    result.error += originalStatus
        ? L"（HTTP " + std::to_wstring(originalStatus) + L"）: " + originalError
        : L"（通信エラー）: " + originalError;
    if (!attempted.empty()) result.error += L"\n代替モデルの結果: " + attempted;
    else result.error += L"\n利用可能な代替モデルがありません。モデル一覧を更新してください。";
    result.notice = L"代替モデルへの自動切り替えを試しました。";
    return result;
}
