#pragma once
#include <string>
#include <vector>

// Provider model IDs are public catalog metadata, not credentials.
// Stored as UTF-8 under %APPDATA%\\Becky-AI-Assistant.
bool LoadProviderModelCatalog(int provider, std::vector<std::wstring>& modelIds,
    std::wstring& error);
bool SaveProviderModelCatalog(int provider, const std::vector<std::wstring>& modelIds,
    std::wstring& error);
