#pragma once
#include <algorithm>
#include <cctype>
#include <string>
namespace provider_model_eligibility {
inline std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
inline bool ContainsAny(const std::string& value, const char* const* terms, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (value.find(terms[i]) != std::string::npos) return true;
    return false;
}
inline bool IsGroqChatCompletionModelEligible(const std::string& modelId) {
    const std::string id = LowerAscii(modelId);
    static const char* const excluded[] = {
        "whisper", "guard", "safeguard", "embedding", "speech", "tts",
        "transcribe", "moderation", "orpheus"
    };
    return !id.empty() && !ContainsAny(id, excluded, sizeof(excluded) / sizeof(excluded[0]));
}
inline bool IsWorkersAIChatCompletionModelEligible(const std::string& modelId) {
    const std::string id = LowerAscii(modelId);
    static const char* const excluded[] = {
        "@cf/cloudflare/clairic", "@cf/cloudflare/clef", "@cf/meta/llama-guard",
        "vision-instruct", "@cf/deepseek-ai/deepseek-v4.1-flash",
        "@cf/thinkingmachines/inkling-2-small", "@cf/swiss-ai/apertus",
        "@cf/utter-project/eurollm", "@cf/zai-org/glm-4.7-flash",
        "@cf/zai-org/glm-5.3"
    };
    return !id.empty() && !ContainsAny(id, excluded, sizeof(excluded) / sizeof(excluded[0]));
}
} // namespace provider_model_eligibility
