#include "providers/ModelDescriptions.h"
#include <cassert>
#include <string>

int main()
{
    const auto oss120 = GetModelDescription(1, L"openai/gpt-oss-120b");
    assert(oss120.find(L"131,072") != std::wstring::npos);
    assert(oss120.find(L"推論") != std::wstring::npos);

    const auto oss20 = GetModelDescription(1, L"openai/gpt-oss-20b");
    assert(oss20.find(L"1,000 tokens/sec") != std::wstring::npos);

    const auto qwen = GetModelDescription(1, L"qwen/qwen3.8-27b");
    assert(qwen.find(L"マルチモーダル") != std::wstring::npos);

    const auto workers = GetModelDescription(2, L"@cf/meta/llama-3.1-8b-instruct");
    assert(workers.find(L"Meta Llama") != std::wstring::npos);
    assert(workers.find(L"未確認") != std::wstring::npos);

    const auto unknown = GetModelDescription(1, L"vendor/new-model");
    assert(unknown.find(L"詳細仕様は未登録") != std::wstring::npos);

    const auto gemini = GetModelDescription(0, L"gemini-3.5-flash-lite");
    assert(gemini.find(L"軽量") != std::wstring::npos);
    return 0;
}
