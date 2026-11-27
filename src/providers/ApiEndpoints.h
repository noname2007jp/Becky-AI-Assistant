#pragma once
#include <string>

// Provider API destinations are intentionally application-managed.
// Never derive a request host from an API response or user-supplied URL.
namespace api_endpoints
{
    inline constexpr wchar_t kGeminiHost[] = L"generativelanguage.googleapis.com";
    inline constexpr wchar_t kGeminiModelsListPath[] = L"/v1beta/models?pageSize=100";
    inline constexpr wchar_t kGeminiModelsCatalogPath[] = L"/v1beta/models?pageSize=1000";
    inline constexpr wchar_t kGeminiGeneratePathPrefix[] = L"/v1beta/models/";
    inline constexpr wchar_t kGeminiGeneratePathSuffix[] = L":generateContent";

    inline constexpr wchar_t kGooglePricingHost[] = L"ai.google.dev";
    inline constexpr wchar_t kGooglePricingPath[] = L"/gemini-api/docs/pricing";

    inline constexpr wchar_t kGroqHost[] = L"api.groq.com";
    inline constexpr wchar_t kGroqModelsPath[] = L"/openai/v1/models";
    inline constexpr wchar_t kGroqChatCompletionsPath[] = L"/openai/v1/chat/completions";

    inline constexpr wchar_t kWorkersAIHost[] = L"api.cloudflare.com";
    inline constexpr wchar_t kWorkersAIModelsPathPrefix[] = L"/client/v4/accounts/";
    inline constexpr wchar_t kWorkersAIModelsPathSuffix[] =
        L"/ai/models/search?task=Text%20Generation&hide_experimental=true&include_deprecated=false&per_page=100&page=1";
    inline constexpr wchar_t kWorkersAIChatPathPrefix[] = L"/client/v4/accounts/";
    inline constexpr wchar_t kWorkersAIChatPathSuffix[] = L"/ai/v1/chat/completions";

    inline constexpr wchar_t kCloudflareGatewayHost[] = L"gateway.ai.cloudflare.com";
    inline constexpr wchar_t kCloudflareGatewayGroqPathPrefix[] = L"/v1/";
    inline constexpr wchar_t kCloudflareGatewayGroqPathMiddle[] = L"/";
    inline constexpr wchar_t kCloudflareGatewayGroqPathSuffix[] = L"/groq/chat/completions";
}
