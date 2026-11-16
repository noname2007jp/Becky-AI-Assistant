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
        IsRetryableGeminiStatus(503) && IsRetryableGeminiStatus(504),
        "transient HTTP statuses are retryable");
    Check(!IsRetryableGeminiStatus(400) && !IsRetryableGeminiStatus(401) &&
        !IsRetryableGeminiStatus(403) && !IsRetryableGeminiStatus(404) &&
        !IsRetryableGeminiStatus(413),
        "configuration and request errors are not retryable");

    const std::vector<GeminiModelOption> models = {
        { L"Free Flash", L"gemini-free-flash", L"MEDIUM", true, true },
        { L"Free Flash-Lite", L"gemini-free-flash-lite", L"LOW", true, true },
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
    Check(recovered.modelUsed == L"gemini-free-flash-lite" && !recovered.notice.empty(),
        "fallback prefers a verified free Flash-Lite model and reports the switch");

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

    return failures == 0 ? 0 : 1;
}
