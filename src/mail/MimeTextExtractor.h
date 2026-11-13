#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// Extracts a human-readable text body from a raw RFC 5322/MIME message.
// Attachments are ignored. The returned text is UTF-16 for the Win32 UI.
bool ExtractMailText(const char* source, size_t sourceLength,
                     std::wstring& output, std::wstring& error);
