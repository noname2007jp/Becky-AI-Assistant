#pragma once

#include <algorithm>
#include <string>
#include <vector>

// These models were observed to be incompatible with the plugin's
// OpenAI-style chat-completions request or unavailable for the tested plan/account.
// Keep this list conservative and exact; catalog presence alone is not eligibility.
inline bool IsWorkersAIChatCompletionsCatalogModel(const std::string& id)
{
    static const std::vector<std::string> unsupported = {
        "@cf/cloudflare/clairic",
        "@cf/cloudflare/clairic-flash",
        "@cf/cloudflare/clef",
        "@cf/cloudflare/clef-flash",
        "@cf/cloudflare/clef-omni",
        "@cf/deepseek-ai/deepseek-v4.1-flash",
        "@cf/meta/llama-3.2-11b-vision-instruct",
        "@cf/meta/llama-guard-3-8b",
        "@cf/swiss-ai/apertus-v1.5-8b",
        "@cf/thinkingmachines/inkling-2-small",
        "@cf/utter-project/eurollm-9b-it",
        "@cf/zai-org/glm-4.7-flash",
        "@cf/zai-org/glm-5.3"
    };
    return std::find(unsupported.begin(), unsupported.end(), id) == unsupported.end();
}

inline bool IsGroqChatCompletionsCatalogModel(const std::string& id)
{
    std::string lower = id;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find("orpheus") == std::string::npos;
}
