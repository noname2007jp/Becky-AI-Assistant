#pragma once
#include <string>

bool LoadGeminiApiKey(std::wstring& apiKey, std::wstring& error);
bool SaveGeminiApiKey(const std::wstring& apiKey, std::wstring& error);
