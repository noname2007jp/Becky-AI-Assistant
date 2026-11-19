#pragma once

#include <string>

enum class AIProvider : unsigned int
{
    Gemini = 0,
    Groq = 1,
    WorkersAI = 2
};

enum class AIConnectionRoute : unsigned int
{
    Direct = 0,
    CloudflareAIGateway = 1
};

struct AIProviderSettings
{
    AIProvider provider = AIProvider::Gemini;
    AIConnectionRoute route = AIConnectionRoute::Direct;

    std::wstring geminiApiKey;
    std::wstring geminiModel = L"gemini-3.5-flash-lite";

    std::wstring groqApiKey;
    std::wstring groqModel;

    std::wstring workersAccountId;
    std::wstring workersApiToken;
    std::wstring workersModel;

    std::wstring cloudflareAccountId;
    std::wstring cloudflareGatewayId;
    std::wstring cloudflareApiToken;

    // Privacy-safe defaults. These are opt-in and independent of provider policy.
    bool gatewayRequestLogging = false;
    bool gatewayCacheEnabled = false;
};

// Reads the new provider store. On first run, imports the existing Gemini key
// and default model without changing or deleting the legacy files.
bool LoadAIProviderSettings(AIProviderSettings& settings, std::wstring& error);

// Saves provider credentials and routing preferences encrypted with Windows DPAPI.
bool SaveAIProviderSettings(const AIProviderSettings& settings, std::wstring& error);
