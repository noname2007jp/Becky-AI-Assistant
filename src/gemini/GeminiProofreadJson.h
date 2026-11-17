#pragma once
#include <string>

// Extracts the first text value from a Gemini JSON response. Shared by all Gemini operations.
// Decodes all JSON escapes, including UTF-16 surrogate pairs in \uXXXX escapes.
bool ExtractGeminiResponseText(const std::string& json, std::wstring& output);
