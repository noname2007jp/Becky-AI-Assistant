#pragma once
#include <string>

// Extracts the first candidate text value from a Gemini JSON response.
// Decodes all JSON escapes, including UTF-16 surrogate pairs in \uXXXX escapes.
bool ExtractGeminiResponseText(const std::string& json, std::wstring& output);
