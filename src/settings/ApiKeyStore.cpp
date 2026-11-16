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

bool LoadPiiMaskTerms(std::vector<std::wstring>& terms, std::wstring& error)
{
    terms.clear();
    error.clear();
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                         nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK)
    {
        error = L"マスキング設定の保存先を取得できません。";
        return false;
    }
    const std::wstring path = std::wstring(appData) + L"\\Becky-AI-Assistant\\pii-mask.dat";
    std::ifstream file(path, std::ios::binary);
    if (!file) return true;
    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size <= 0 || size > 1024 * 1024) { error = L"マスキング設定ファイルのサイズが不正です。"; return false; }
    file.seekg(0, std::ios::beg);
    std::vector<BYTE> encrypted(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(encrypted.data()), size)) { error = L"マスキング設定を読み込めません。"; return false; }
    DATA_BLOB input{ static_cast<DWORD>(encrypted.size()), encrypted.data() };
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
    { error = L"マスキング設定を復号できません。"; return false; }
    if (output.cbData % sizeof(wchar_t) != 0 || output.cbData > 512 * 1024)
    {
        SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
        error = L"マスキング設定の形式が不正です."; return false;
    }
    std::wstring all(reinterpret_cast<wchar_t*>(output.pbData), output.cbData / sizeof(wchar_t));
    SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
    size_t start = 0;
    while (start < all.size())
    {
        size_t end = all.find(L'\n', start);
        if (end == std::wstring::npos) end = all.size();
        std::wstring term = all.substr(start, end - start);
        if (!term.empty() && term.back() == L'\r') term.pop_back();
        if (term.size() >= 2) terms.push_back(term);
        start = end + 1;
    }
    return true;
}

bool SavePiiMaskTerms(const std::vector<std::wstring>& terms, std::wstring& error)
{
    error.clear();
    std::wstring all;
    for (const auto& term : terms)
    {
        if (term.size() < 2) continue;
        if (term.find_first_of(L"\r\n") != std::wstring::npos) continue;
        all += term + L"\n";
    }
    if (all.size() > 65536) { error = L"マスキング語句が多すぎます。"; return false; }
    if (all.empty()) all = L"\n";
    DATA_BLOB input{ static_cast<DWORD>(all.size() * sizeof(wchar_t)),
        reinterpret_cast<BYTE*>(all.data()) };
    DATA_BLOB encrypted{};
    if (!CryptProtectData(&input, L"Becky AI Assistant PII mask terms",
        nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted))
    { error = L"マスキング設定の暗号化に失敗しました。"; return false; }
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                         nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK)
    {
        SecureZeroMemory(encrypted.pbData, encrypted.cbData); LocalFree(encrypted.pbData);
        error = L"マスキング設定の保存先を取得できません。"; return false;
    }
    const std::wstring directory = std::wstring(appData) + L"\\Becky-AI-Assistant";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        SecureZeroMemory(encrypted.pbData, encrypted.cbData); LocalFree(encrypted.pbData);
        error = L"マスキング設定の保存先を作成できません。"; return false;
    }
    const std::wstring path = directory + L"\\pii-mask.dat";
    const std::wstring temp = path + L".tmp";
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    const bool written = file && static_cast<bool>(file.write(
        reinterpret_cast<const char*>(encrypted.pbData), encrypted.cbData));
    file.close();
    SecureZeroMemory(encrypted.pbData, encrypted.cbData); LocalFree(encrypted.pbData);
    if (!written || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temp.c_str()); error = L"マスキング設定を保存できません。"; return false;
    }
    return true;
}

bool LoadPiiMaskOptions(PiiMaskOptions& options, std::wstring& error)
{
    options = PiiMaskOptions{};
    error.clear();
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK)
    { error = L"マスキング設定の保存先を取得できません。"; return false; }
    const std::wstring path = std::wstring(appData) + L"\\Becky-AI-Assistant\\pii-options.dat";
    std::ifstream file(path, std::ios::binary);
    if (!file) return true;
    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size <= 0 || size > 1024 * 1024)
    { error = L"マスキング対象設定のサイズが不正です。"; return false; }
    file.seekg(0, std::ios::beg);
    std::vector<BYTE> encrypted(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(encrypted.data()), size))
    { error = L"マスキング対象設定を読み込めません。"; return false; }
    DATA_BLOB input{ static_cast<DWORD>(encrypted.size()), encrypted.data() };
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
        CRYPTPROTECT_UI_FORBIDDEN, &output))
    { error = L"マスキング対象設定を復号できません。"; return false; }
    if (output.cbData != 9 && output.cbData != 10)
    {
        SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
        error = L"マスキング対象設定の形式が不正です。"; return false;
    }
    const BYTE* p = output.pbData;
    // Legacy 9-byte settings remain compatible; global masking defaults OFF.
    options.enabled = output.cbData == 10 ? p[0] != 0 : false;
    const size_t offset = output.cbData == 10 ? 1 : 0;
    options.email = p[offset + 0] != 0; options.phone = p[offset + 1] != 0;
    options.postal = p[offset + 2] != 0; options.ip = p[offset + 3] != 0;
    options.company = p[offset + 4] != 0; options.person = p[offset + 5] != 0;
    options.address = p[offset + 6] != 0; options.account = p[offset + 7] != 0;
    options.custom = p[offset + 8] != 0;
    SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
    return true;
}

bool SavePiiMaskOptions(const PiiMaskOptions& options, std::wstring& error)
{
    error.clear();
    BYTE flags[10] = {
        static_cast<BYTE>(options.enabled), static_cast<BYTE>(options.email),
        static_cast<BYTE>(options.phone), static_cast<BYTE>(options.postal),
        static_cast<BYTE>(options.ip), static_cast<BYTE>(options.company),
        static_cast<BYTE>(options.person), static_cast<BYTE>(options.address),
        static_cast<BYTE>(options.account), static_cast<BYTE>(options.custom)
    };
    DATA_BLOB input{ sizeof(flags), flags };
    DATA_BLOB encrypted{};
    if (!CryptProtectData(&input, L"Becky AI Assistant PII mask options",
        nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted))
    { error = L"マスキング対象設定の暗号化に失敗しました。"; return false; }
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK)
    {
        SecureZeroMemory(encrypted.pbData, encrypted.cbData); LocalFree(encrypted.pbData);
        error = L"マスキング設定の保存先を取得できません。"; return false;
    }
    const std::wstring directory = std::wstring(appData) + L"\\Becky-AI-Assistant";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        SecureZeroMemory(encrypted.pbData, encrypted.cbData); LocalFree(encrypted.pbData);
        error = L"マスキング設定の保存先を作成できません。"; return false;
    }
    const std::wstring path = directory + L"\\pii-options.dat";
    const std::wstring temp = path + L".tmp";
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    const bool written = file && static_cast<bool>(file.write(
        reinterpret_cast<const char*>(encrypted.pbData), encrypted.cbData));
    file.close();
    SecureZeroMemory(encrypted.pbData, encrypted.cbData); LocalFree(encrypted.pbData);
    if (!written || !MoveFileExW(temp.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temp.c_str()); error = L"マスキング対象設定を保存できません。"; return false;
    }
    return true;
}


bool LoadGeminiReliabilityOptions(GeminiReliabilityOptions& options, std::wstring& error)
{
    options = GeminiReliabilityOptions{};
    error.clear();
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK)
    { error = L"安定性設定の保存先を取得できません。"; return false; }
    const std::wstring path = std::wstring(appData) + L"\\Becky-AI-Assistant\\reliability-options.dat";
    std::ifstream file(path, std::ios::binary);
    if (!file) return true;
    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size <= 0 || size > 1024)
    { error = L"安定性設定ファイルのサイズが不正です。"; return false; }
    file.seekg(0, std::ios::beg);
    std::vector<BYTE> encrypted(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(encrypted.data()), size))
    { error = L"安定性設定を読み込めません。"; return false; }
    DATA_BLOB input{ static_cast<DWORD>(encrypted.size()), encrypted.data() };
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
        CRYPTPROTECT_UI_FORBIDDEN, &output))
    { error = L"安定性設定を復号できません。"; return false; }
    if (output.cbData != 1 && output.cbData != 2)
    {
        SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
        error = L"安定性設定の形式が不正です。"; return false;
    }
    options.automaticModelFallback = output.pbData[0] != 0;\n    options.debugLogging = output.cbData >= 2 && output.pbData[1] != 0;
    SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
    return true;
}

bool SaveGeminiReliabilityOptions(const GeminiReliabilityOptions& options, std::wstring& error)
{
    error.clear();
    BYTE value = static_cast<BYTE>(options.automaticModelFallback);
    DATA_BLOB input{ 1, &value };
    DATA_BLOB encrypted{};
    if (!CryptProtectData(&input, L"Becky AI Assistant reliability options",
        nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted))
    { error = L"安定性設定の暗号化に失敗しました。"; return false; }
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK)
    {
        SecureZeroMemory(encrypted.pbData, encrypted.cbData); LocalFree(encrypted.pbData);
        error = L"安定性設定の保存先を取得できません。"; return false;
    }
    const std::wstring directory = std::wstring(appData) + L"\\Becky-AI-Assistant";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        SecureZeroMemory(encrypted.pbData, encrypted.cbData); LocalFree(encrypted.pbData);
        error = L"安定性設定の保存先を作成できません。"; return false;
    }
    const std::wstring path = directory + L"\\reliability-options.dat";
    const std::wstring temp = path + L".tmp";
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    const bool written = file && static_cast<bool>(file.write(
        reinterpret_cast<const char*>(encrypted.pbData), encrypted.cbData));
    file.close();
    SecureZeroMemory(encrypted.pbData, encrypted.cbData); LocalFree(encrypted.pbData);
    if (!written || !MoveFileExW(temp.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temp.c_str()); error = L"安定性設定を保存できません。"; return false;
    }
    return true;
}
