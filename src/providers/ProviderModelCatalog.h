#pragma once
#include <string>
#include <vector>

// Fetches active text-generation model IDs from provider-owned model catalog APIs.
// Free-tier eligibility is deliberately not inferred from these endpoints.
bool FetchGroqModelCatalog(const std::wstring& apiKey,
    std::vector<std::wstring>& modelIds, std::wstring& error);
bool FetchWorkersAIModelCatalog(const std::wstring& accountId,
    const std::wstring& apiToken, std::vector<std::wstring>& modelIds,
    std::wstring& error);
