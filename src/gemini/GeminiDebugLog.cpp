#include "GeminiDebugLog.h"
#include "settings/ApiKeyStore.h"
#include <windows.h>
#include <shlobj.h>
#include <string>

namespace
{
std::string ToUtf8(const std::wstring& value)
{
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}
bool GetLogPath(std::wstring& path)
{
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK) return false;
    const std::wstring directory = std::wstring(appData) + L"\\Becky-AI-Assistant";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    path = directory + L"\\debug.log";
    return true;
}
void Append(const std::string& text)
{
    std::wstring path;
    if (!GetLogPath(path)) return;
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\BeckyAIAssistantDebugLog");
    if (mutex) WaitForSingleObject(mutex, INFINITE);
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
    {
        const ULONGLONG size = (static_cast<ULONGLONG>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
        if (size > 2 * 1024 * 1024)
        {
            const std::wstring backup = path + L".1";
            DeleteFileW(backup.c_str());
            MoveFileExW(path.c_str(), backup.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    }
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        CloseHandle(file);
    }
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
}
std::string Header(const char* event, const std::wstring& operation, const std::wstring& modelId)
{
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t line[1024]{};
    swprintf_s(line, L"[%04u-%02u-%02u %02u:%02u:%02u] %S operation=%s model=%s",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        event, operation.c_str(), modelId.c_str());
    return ToUtf8(line) + "\r\n";
}
}
void LogGeminiRequest(const std::wstring& operation, const std::wstring& modelId,
    bool maskingEnabled, size_t maskedTermCount, const std::string& payload)
{
    GeminiReliabilityOptions options;
    std::wstring error;
    if (!LoadGeminiReliabilityOptions(options, error) || !options.debugLogging) return;
    std::string entry = Header("REQUEST", operation, modelId);
    entry += "masking_enabled=" + std::string(maskingEnabled ? "true" : "false") +
        " masked_terms=" + std::to_string(maskedTermCount) +
        " payload_bytes=" + std::to_string(payload.size()) + "\r\n";
    if (maskingEnabled)
    {
        entry += "----- MASKED REQUEST PAYLOAD BEGIN -----\r\n";
        entry += payload;
        entry += "\r\n----- MASKED REQUEST PAYLOAD END -----\r\n";
    }
    else entry += "payload_omitted=masking_disabled\r\n";
    Append(entry);
}
void LogGeminiResponse(const std::wstring& operation, const std::wstring& modelId,
    unsigned long httpStatus, bool success, const std::wstring& error)
{
    GeminiReliabilityOptions options;
    std::wstring loadError;
    if (!LoadGeminiReliabilityOptions(options, loadError) || !options.debugLogging) return;
    std::string entry = Header("RESPONSE", operation, modelId);
    entry += "http_status=" + std::to_string(httpStatus) +
        " success=" + std::string(success ? "true" : "false") + "\r\n";
    if (!error.empty()) entry += "error=" + ToUtf8(error) + "\r\n";
    Append(entry);
}
