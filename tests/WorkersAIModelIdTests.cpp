#include "providers/WorkersAIModelId.h"
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
    return 0;
}
