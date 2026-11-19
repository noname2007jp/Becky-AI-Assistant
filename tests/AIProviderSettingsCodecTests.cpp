#include "settings/AIProviderSettingsCodec.h"

#include <iostream>
#include <vector>
#include <string>

namespace
{
int failures = 0;
void Check(bool condition, const char* name)
{
    if (!condition) { std::cerr << "[FAIL] " << name << "\n"; ++failures; }
    else std::cout << "[PASS] " << name << "\n";
}
}

int main()
{
    AIProviderSettings original;
    original.provider = AIProvider::WorkersAI;
    original.route = AIConnectionRoute::CloudflareAIGateway;
    original.geminiApiKey = L"legacy-gemini-key";
    original.geminiModel = L"gemini-3.5-flash-lite";
    original.groqApiKey = L"groq-key";
    original.groqModel = L"llama-model";
    original.workersAccountId = L"account-123";
    original.workersApiToken = L"workers-token";
    original.workersModel = L"@cf/model";
    original.cloudflareAccountId = L"cf-account";
    original.cloudflareGatewayId = L"mail-gateway";
    original.cloudflareApiToken = L"cf-token";
    original.gatewayRequestLogging = false;
    original.gatewayCacheEnabled = false;

    std::vector<std::uint8_t> encoded;
    Check(AIProviderSettingsCodec::Encode(original, encoded), "encode valid settings");
    AIProviderSettings decoded;
    Check(AIProviderSettingsCodec::Decode(encoded.data(), encoded.size(), decoded),
          "decode valid settings");
    Check(decoded.provider == original.provider && decoded.route == original.route,
          "provider and route round-trip");
    Check(decoded.groqApiKey == original.groqApiKey &&
          decoded.workersApiToken == original.workersApiToken &&
          decoded.cloudflareApiToken == original.cloudflareApiToken,
          "credentials round-trip");
    Check(decoded.geminiModel == original.geminiModel &&
          decoded.workersModel == original.workersModel,
          "model IDs round-trip");
    Check(!decoded.gatewayRequestLogging && !decoded.gatewayCacheEnabled,
          "privacy settings default to off");

    auto truncated = encoded;
    truncated.pop_back();
    Check(!AIProviderSettingsCodec::Decode(truncated.data(), truncated.size(), decoded),
          "reject truncated data");

    auto badVersion = encoded;
    badVersion[4] = 99;
    Check(!AIProviderSettingsCodec::Decode(badVersion.data(), badVersion.size(), decoded),
          "reject unknown format version");

    AIProviderSettings invalid;
    invalid.provider = static_cast<AIProvider>(99);
    Check(!AIProviderSettingsCodec::Encode(invalid, encoded), "reject invalid provider");

    if (failures) std::cerr << failures << " test(s) failed\n";
    return failures == 0 ? 0 : 1;
}
