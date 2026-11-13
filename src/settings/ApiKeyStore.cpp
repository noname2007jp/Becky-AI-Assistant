#include "ApiKeyStore.h"

#include <windows.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <fstream>
#include <vector>

#pragma comment(lib, "crypt32.lib")

namespace
{
bool GetStorePath(std::wstring& path)
{
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                         nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK)
        return false;

    std::wstring directory = std::wstring(appData) + L"\\Becky-AI-Assistant";
    if (!CreateDirectoryW(directory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
        return false;

    path = directory + L"\\gemini-key.dat";
    return true;
}

bool ReadEncrypted(std::vector<BYTE>& bytes)
{
    std::wstring path;
    if (!GetStorePath(path)) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size <= 0 || size > 1024 * 1024) return false;
    file.seekg(0, std::ios::beg);
    bytes.resize(static_cast<size_t>(size));
    return static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), size));
}
}

bool LoadGeminiApiKey(std::wstring& apiKey, std::wstring& error)
{
    apiKey.clear();
    error.clear();
    std::vector<BYTE> encrypted;
    if (!ReadEncrypted(encrypted))
        return true; // No saved key is a normal first-run state.

    DATA_BLOB input{ static_cast<DWORD>(encrypted.size()), encrypted.data() };
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        error = L"保存済みAPIキーを復号できませんでした。Windowsユーザーが変わった可能性があります。";
        return false;
    }

    if (output.cbData % sizeof(wchar_t) != 0 || output.cbData > 65536)
    {
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);
        error = L"保存データの形式が正しくありません。";
        return false;
    }
    apiKey.assign(reinterpret_cast<wchar_t*>(output.pbData),
                  output.cbData / sizeof(wchar_t));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return true;
}

bool SaveGeminiApiKey(const std::wstring& apiKey, std::wstring& error)
{
    error.clear();
    if (apiKey.empty())
    {
        error = L"APIキーが入力されていません。";
        return false;
    }
    if (apiKey.size() > 4096)
    {
        error = L"APIキーが長すぎます。";
        return false;
    }

    DATA_BLOB input{
        static_cast<DWORD>(apiKey.size() * sizeof(wchar_t)),
        reinterpret_cast<BYTE*>(const_cast<wchar_t*>(apiKey.data()))
    };
    DATA_BLOB encrypted{};
    if (!CryptProtectData(&input, L"Becky AI Assistant Gemini API key",
                          nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                          &encrypted))
    {
        error = L"Windows DPAPIによるAPIキーの暗号化に失敗しました。";
        return false;
    }

    std::wstring path;
    if (!GetStorePath(path))
    {
        SecureZeroMemory(encrypted.pbData, encrypted.cbData);
        LocalFree(encrypted.pbData);
        error = L"設定保存先を作成できませんでした。";
        return false;
    }

    const std::wstring tempPath = path + L".tmp";
    std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
    const bool written = file &&
        static_cast<bool>(file.write(reinterpret_cast<const char*>(encrypted.pbData),
                                     encrypted.cbData));
    file.close();
    SecureZeroMemory(encrypted.pbData, encrypted.cbData);
    LocalFree(encrypted.pbData);
    if (!written || !MoveFileExW(tempPath.c_str(), path.c_str(),
                                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(tempPath.c_str());
        error = L"暗号化したAPIキーを保存できませんでした。";
        return false;
    }
    return true;
}
