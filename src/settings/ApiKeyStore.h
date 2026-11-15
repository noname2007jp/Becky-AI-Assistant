#pragma once
#include <string>
#include <vector>

bool LoadGeminiApiKey(std::wstring& apiKey, std::wstring& error);
bool SaveGeminiApiKey(const std::wstring& apiKey, std::wstring& error);
bool LoadPiiMaskTerms(std::vector<std::wstring>& terms, std::wstring& error);
bool SavePiiMaskTerms(const std::vector<std::wstring>& terms, std::wstring& error);
