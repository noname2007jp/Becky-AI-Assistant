#include "ProviderModelEligibility.h"
#include <cassert>
int main() {
    using provider_model_eligibility::IsGroqChatCompletionModelEligible;
    using provider_model_eligibility::IsWorkersAIChatCompletionModelEligible;
    assert(IsGroqChatCompletionModelEligible("allam-2-7b"));
    assert(IsGroqChatCompletionModelEligible("qwen/qwen3.8-27b"));
    assert(!IsGroqChatCompletionModelEligible("canopylabs/orpheus-v1-english"));
    assert(!IsGroqChatCompletionModelEligible("whisper-large-v3"));
    assert(provider_model_eligibility::IsGroqChatCompletionModelEligible(std::wstring(L"allam-2-7b")));
    assert(!provider_model_eligibility::IsGroqChatCompletionModelEligible(std::wstring(L"canopylabs/orpheus-v1-english")));
    assert(IsWorkersAIChatCompletionModelEligible("@cf/mistral/mistral-7b-instruct-v0.2"));
    assert(IsWorkersAIChatCompletionModelEligible("@cf/meta/llama-3.3-70b-instruct-fp8-fast"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/cloudflare/clairic"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/cloudflare/clef-flash"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/meta/llama-guard-3-8b"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/meta/llama-3.2-11b-vision-instruct"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/deepseek-ai/deepseek-v4.1-flash"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/thinkingmachines/inkling-2-small"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/swiss-ai/apertus-v1.5-8b"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/utter-project/eurollm-9b-it"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/zai-org/glm-4.7-flash"));
    assert(!IsWorkersAIChatCompletionModelEligible("@cf/zai-org/glm-5.3"));
    assert(!provider_model_eligibility::IsWorkersAIChatCompletionModelEligible(std::wstring(L"@cf/cloudflare/clairic-flash")));
    return 0;
}