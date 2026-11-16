#pragma once
#include <string>
#include <vector>

bool LoadGeminiApiKey(std::wstring& apiKey, std::wstring& error);
bool SaveGeminiApiKey(const std::wstring& apiKey, std::wstring& error);
bool LoadPiiMaskTerms(std::vector<std::wstring>& terms, std::wstring& error);
bool SavePiiMaskTerms(const std::vector<std::wstring>& terms, std::wstring& error);

struct PiiMaskOptions
{
    bool email = true;
    bool phone = true;
    bool postal = true;
    bool ip = true;
    bool company = true;
    bool person = true;
    bool address = true;
    bool account = true;
    bool custom = true;
};
bool LoadPiiMaskOptions(PiiMaskOptions& options, std::wstring& error);
bool SavePiiMaskOptions(const PiiMaskOptions& options, std::wstring& error);
