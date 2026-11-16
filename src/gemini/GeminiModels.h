#pragma once
#include <string>
#include <vector>

struct GeminiModelOption
{
    std::wstring label;
    std::wstring id;
    std::wstring thinking;
    bool freeTier = false;
};

std::vector<GeminiModelOption> DefaultGeminiModels();
bool LoadGeminiModelCatalog(std::vector<GeminiModelOption>& models, std::wstring& error);
bool SaveGeminiModelCatalog(const std::vector<GeminiModelOption>& models, std::wstring& error);
bool FetchGeminiModelCatalog(const std::wstring& apiKey, bool includePaid,
    std::vector<GeminiModelOption>& models, std::wstring& error);
