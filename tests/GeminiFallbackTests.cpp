#include "gemini/GeminiFallback.h"
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void Check(bool condition, const char* name)
{
    if (condition) std::cout << "[PASS] " << name << "\n";
    else { std::cout << "[FAIL] " << name << "\n"; ++failures; }
}

GeminiGenerateResult Failure(unsigned long status, bool retryable)
{
    GeminiGenerateResult result{ false, status, {}, L"test error" };
    result.retryable = retryable;
    return result;
}

GeminiGenerateResult Success(const std::wstring& text)
{
    GeminiGenerateResult result{ true, 200, text, L"" };
    return result;
}
}

int main()
{
    Check(IsRetryableGeminiStatus(408) && IsRetryableGeminiStatus(429) &&
        IsRetryableGeminiStatus(500) && IsRetryableGeminiStatus(502) &&
        IsRetryableGeminiStatus(501) && IsRetryableGeminiStatus(503) &&
        IsRetryableGeminiStatus(504) && IsRetryableGeminiStatus(599),
        "transient HTTP statuses are retryable");
    Check(!IsRetryableGeminiStatus(400) && !IsRetryableGeminiStatus(401) &&
        !IsRetryableGeminiStatus(403) && IsRetryableGeminiStatus(404) &&
        !IsRetryableGeminiStatus(413),
        "403 is not retryable while 404 can trigger an alternate model");
    Check(IsRetryableWinHttpError(ERROR_WINHTTP_TIMEOUT) &&
        IsRetryableWinHttpError(ERROR_WINHTTP_CANNOT_CONNECT) &&
        IsRetryableWinHttpError(ERROR_WINHTTP_CONNECTION_ERROR) &&
        IsRetryableWinHttpError(ERROR_WINHTTP_NAME_NOT_RESOLVED) &&
        !IsRetryableWinHttpError(ERROR_WINHTTP_SECURE_FAILURE),
        "transient WinHTTP transport failures are retryable but TLS configuration errors are not");

    const std::vector<GeminiModelOption> models = {
        { L"Gemini 2.5 Flash-Lite", L"gemini-2.5-flash-lite", L"MEDIUM", true, true },
        { L"Gemini 3.5 Flash-Lite", L"gemini-3.5-flash-lite", L"MEDIUM", true, true },
        { L"Gemini 3.8 Flash", L"gemini-3.8-flash", L"MEDIUM", true, true },
        { L"Unknown Flash", L"gemini-unknown-flash", L"MEDIUM", false, false },
        { L"Paid Flash", L"gemini-paid-flash", L"MEDIUM", false, true }
    };

    int calls = 0;
    auto recovered = GenerateWithModelFallback(
        L"gemini-primary", L"MEDIUM", models, true,
        [&](const std::wstring& id, const std::wstring&) {
            ++calls;
            return id == L"gemini-primary" ? Failure(503, true) : Success(L"recovered");
        });
    Check(calls == 2 && recovered.success, "temporary failure retries with one alternate model");
    Check(recovered.modelUsed == L"gemini-3.8-flash" && !recovered.notice.empty(),
        "fallback prefers current 3.8 Flash and reports the switch");

    calls = 0;
    std::vector<std::wstring> tried;
    auto skipLegacy404 = GenerateWithModelFallback(
        L"gemini-primary", L"MEDIUM", models, true,
        [&](const std::wstring& id, const std::wstring&) {
            ++calls; tried.push_back(id);
            if (id == L"gemini-primary") return Failure(503, true);
            if (id == L"gemini-3.8-flash") return Failure(404, false);
            return Success(L"recovered on second candidate");
        });
    Check(calls == 3 && skipLegacy404.success &&
        skipLegacy404.modelUsed == L"gemini-3.5-flash-lite",
        "404 from first fallback skips to next current free-tier model");
    Check(!skipLegacy404.notice.empty() &&
        skipLegacy404.notice.find(L"Gemini 2.5") == std::wstring::npos,
        "successful fallback notice identifies the actual selected model");

    Check(GeminiFallbackPriority(L"gemini-2.5-flash-lite") < 0 &&
        GeminiFallbackPriority(L"gemini-3.8-flash") > GeminiFallbackPriority(L"gemini-3.5-flash-lite"),
        "legacy 2.5 models are excluded and current models are prioritized");

    calls = 0;
    auto primary404 = GenerateWithModelFallback(
        L"gemini-2.5-flash-lite", L"MEDIUM", models, true,
        [&](const std::wstring& id, const std::wstring&) {
            ++calls;
            if (id == L"gemini-2.5-flash-lite") return Failure(404, true);
            return Success(L"modern model response");
        });
    Check(calls == 2 && primary404.success &&
        primary404.modelUsed == L"gemini-3.8-flash",
        "primary model 404 falls back to a current model");
    Check(primary404.notice.find(L"gemini-2.5-flash-lite") != std::wstring::npos,
        "success notice names the original model that failed");


    calls = 0;
    std::vector<std::wstring> qualityTried;
    auto placeholderRetry = GenerateWithModelFallback(
        L"gemini-primary", L"MEDIUM", models, true,
        [&](const std::wstring& id, const std::wstring&) {
            ++calls; qualityTried.push_back(id);
            if (id == L"gemini-primary") return Failure(0, true); // network timeout
            if (id == L"gemini-3.8-flash") {
                auto missingPlaceholder = Failure(200, true);
                missingPlaceholder.error = L"masked placeholder missing";
                return missingPlaceholder;
            }
            return Success(L"all placeholders preserved");
        });
    Check(calls == 3 && placeholderRetry.success &&
        placeholderRetry.modelUsed == L"gemini-3.5-flash-lite",
        "placeholder validation failure retries the next verified model");
    Check(placeholderRetry.notice.find(L"応答内容の検証に失敗") != std::wstring::npos,
        "fallback notice explains placeholder validation failure");

    calls = 0;
    auto networkZero = GenerateWithModelFallback(
        L"gemini-2.5-pro", L"MEDIUM", models, true,
        [&](const std::wstring& id, const std::wstring&) {
            ++calls;
            if (id == L"gemini-2.5-pro") return Failure(404, true);
            if (id == L"gemini-3.8-flash") {
                auto networkFailure = Failure(0, true);
                networkFailure.error = L"接続できません（Windowsエラー 12029）";
                return networkFailure;
            }
            return Success(L"recovered after transport error");
        });
    Check(calls == 3 && networkZero.success &&
        networkZero.modelUsed == L"gemini-3.5-flash-lite",
        "HTTP 0 transport failure continues to the next verified fallback model");

    calls = 0;
    auto disabled = GenerateWithModelFallback(
        L"gemini-primary", L"MEDIUM", models, false,
        [&](const std::wstring&, const std::wstring&) {
            ++calls; return Failure(503, true);
        });
    Check(calls == 1 && !disabled.success, "fallback can be disabled");

    calls = 0;
    auto forbidden = GenerateWithModelFallback(
        L"gemini-primary", L"MEDIUM", models, true,
        [&](const std::wstring&, const std::wstring&) {
            ++calls; return Failure(403, false);
        });
    Check(calls == 1 && !forbidden.success, "403 does not trigger a model switch");

    calls = 0;
    const std::vector<GeminiModelOption> unverified = {
        { L"Unknown Flash", L"gemini-unknown-flash", L"MEDIUM", false, false },
        { L"Paid Flash", L"gemini-paid-flash", L"MEDIUM", false, true }
    };
    auto noPaidFallback = GenerateWithModelFallback(
        L"gemini-primary", L"MEDIUM", unverified, true,
        [&](const std::wstring&, const std::wstring&) {
            ++calls; return Failure(429, true);
        });
    Check(calls == 1 && !noPaidFallback.success,
        "paid or unverified models are never selected automatically");

    std::atomic<int> cancelled{ 1 };
    calls = 0;
    auto cancelledResult = GenerateWithModelFallback(
        L"gemini-primary", L"MEDIUM", models, true,
        [&](const std::wstring&, const std::wstring&) {
            ++calls; return Failure(503, true);
        }, &cancelled);
    Check(calls == 1 && !cancelledResult.success,
        "cancellation prevents starting an alternate model");

    calls = 0;
    auto noFreeCandidate = GenerateWithModelFallback(
        L"gemini-primary", L"MEDIUM", unverified, true,
        [&](const std::wstring&, const std::wstring&) {
            ++calls; return Failure(503, true);
        });
    Check(calls == 1 && noFreeCandidate.error.find(L"モデル一覧を更新") != std::wstring::npos,
        "missing verified free models produces an actionable message");

    return failures == 0 ? 0 : 1;
}
