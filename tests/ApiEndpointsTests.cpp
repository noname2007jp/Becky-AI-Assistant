#include "providers/ApiEndpoints.h"
#include "providers/WorkersAIEndpoint.h"

#include <cassert>
#include <string>

int main()
{
    assert(std::wstring(api_endpoints::kGeminiHost) == L"generativelanguage.googleapis.com");
    assert(std::wstring(api_endpoints::kGeminiModelsListPath) == L"/v1beta/models?pageSize=100");
    assert(std::wstring(api_endpoints::kGeminiGeneratePathPrefix) + L"gemini-test" +
        api_endpoints::kGeminiGeneratePathSuffix ==
        L"/v1beta/models/gemini-test:generateContent");

    assert(std::wstring(api_endpoints::kGooglePricingHost) == L"ai.google.dev");
    assert(std::wstring(api_endpoints::kGooglePricingPath) == L"/gemini-api/docs/pricing");

    assert(std::wstring(api_endpoints::kGroqHost) == L"api.groq.com");
    assert(std::wstring(api_endpoints::kGroqModelsPath) == L"/openai/v1/models");
    assert(std::wstring(api_endpoints::kGroqChatCompletionsPath) ==
        L"/openai/v1/chat/completions");

    const std::wstring account = L"0123456789abcdef0123456789abcdef";
    assert(BuildWorkersAIChatCompletionsPath(account) ==
        L"/client/v4/accounts/0123456789abcdef0123456789abcdef/ai/v1/chat/completions");
    assert(std::wstring(api_endpoints::kWorkersAIHost) == L"api.cloudflare.com");
    assert(std::wstring(api_endpoints::kWorkersAIModelsPathPrefix) + account +
        api_endpoints::kWorkersAIModelsPathSuffix ==
        L"/client/v4/accounts/0123456789abcdef0123456789abcdef/ai/models/search?task=Text%20Generation&hide_experimental=true&include_deprecated=false&per_page=100&page=1");

    return 0;
}
