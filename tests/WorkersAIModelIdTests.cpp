#include "providers/WorkersAIModelId.h"
#include "providers/WorkersAIEndpoint.h"
#include <cassert>
#include <string>

int main()
{
    // Prefer callable model name over a numeric catalog record ID.
    assert(SelectWorkersAIModelIdentifier("@cf/meta/llama-3.1-8b-instruct", "12345678901234567890")
        == "@cf/meta/llama-3.1-8b-instruct");

    // Reject numeric IDs when no callable model name is present.
    assert(SelectWorkersAIModelIdentifier("", "12345678901234567890").empty());

    // Accept a namespaced model ID as a fallback for API variants.
    assert(SelectWorkersAIModelIdentifier("", "@cf/qwen/qwen2.5-7b-instruct")
        == "@cf/qwen/qwen2.5-7b-instruct");

    // Also accept provider/model identifiers if Cloudflare adds marketplace models.
    assert(SelectWorkersAIModelIdentifier("meta/llama-model", "987654321") == "meta/llama-model");
    assert(IsValidWorkersAIAccountId(L"0123456789abcdef0123456789abcdef"));
    assert(IsValidWorkersAIAccountId(L"0123456789ABCDEF0123456789ABCDEF"));
    assert(!IsValidWorkersAIAccountId(L""));
    assert(!IsValidWorkersAIAccountId(L"0123456789abcdef0123456789abcdeg"));
    assert(!IsValidWorkersAIAccountId(L"0123456789abcdef0123456789abcdef/path"));
    assert(BuildWorkersAIChatCompletionsPath(L"0123456789abcdef0123456789abcdef") ==
        L"/client/v4/accounts/0123456789abcdef0123456789abcdef/ai/v1/chat/completions");
    return 0;
}
