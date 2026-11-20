#pragma once

#include <string>
#include <vector>

struct ModelCatalogPolicyEntry
{
    int provider = 0; // 0=Gemini, 1=Groq, 2=Workers AI
    std::wstring modelId;
    std::wstring status;
    std::wstring quality;
    std::wstring reason;
    std::wstring replacementModelId;
};

// Loads the last-known-good policy cache. Missing cache is not an error.
bool LoadModelCatalogPolicy(std::wstring& error);

// Downloads the public, provider-neutral policy and atomically replaces the cache.
// This operation performs network I/O and must run on a worker thread.
bool RefreshModelCatalogPolicy(std::wstring& error);

// Returns a concise Japanese advisory. Unknown models are explicitly unassessed.
std::wstring GetModelCatalogPolicyNote(int provider, const std::wstring& modelId);

// Parses and validates a policy document without touching disk or the network.
bool ParseModelCatalogPolicyJson(const std::string& json,
    std::vector<ModelCatalogPolicyEntry>& entries);
