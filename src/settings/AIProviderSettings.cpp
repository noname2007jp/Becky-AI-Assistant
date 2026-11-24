#include "AIProviderSettings.h"
#include "AIProviderSettingsCodec.h"
#include "ApiKeyStore.h"

#include <windows.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <fstream>
#include <vector>

#pragma comment(lib, "crypt32.lib")

namespace
{
bool GetProviderStorePath(std::wstring& path, bool createDirectory)
{
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | (createDirectory ? CSIDL_FLAG_CREATE : 0),
        nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK)
        return false;

    const std::wstring directory = std::wstring(appData) + L"\\Becky-AI-Assistant";
    if (createDirectory && !CreateDirectoryW(directory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    path = directory + L"\\provider-settings.dat";
    return true;
}

void ClearBytes(std::vector<std::uint8_t>& bytes)
{
    if (!bytes.empty()) SecureZeroMemory(bytes.data(), bytes.size());
    bytes.clear();
}
}

bool LoadAIProviderSettings(AIProviderSettings& settings, std::wstring& error)
{
    settings = AIProviderSettings{};
    error.clear();

    std::wstring path;
    if (!GetProviderStorePath(path, false))
    {
        error = L"AIサービス設定の保存先を取得できません。";
        return false;
    }

    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD status = GetLastError();
        if (status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND)
        {
            error = L"AIサービス設定ファイルを確認できません。";
            return false;
        }

        // One-time, read-only compatibility import. Existing Gemini files are
        // deliberately retained so the current Gemini runtime remains intact.
        if (!LoadGeminiApiKey(settings.geminiApiKey, error)) return false;
        GeminiReliabilityOptions reliability;
        if (!LoadGeminiReliabilityOptions(reliability, error)) return false;
        settings.geminiModel = reliability.defaultModel;
        settings.provider = AIProvider::Gemini;
        settings.route = AIConnectionRoute::Direct;
        return true;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        error = L"AIサービス設定ファイルを読み込めません。";
        return false;
    }
    file.seekg(0, std::ios::end);
    const auto fileSize = file.tellg();
    if (fileSize <= 0 || fileSize > 1024 * 1024)
    {
        error = L"AIサービス設定ファイルのサイズが不正です。";
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<BYTE> encrypted(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(encrypted.data()), fileSize))
    {
        error = L"AIサービス設定ファイルを完全に読み込めません。";
        return false;
    }

    DATA_BLOB input{ static_cast<DWORD>(encrypted.size()), encrypted.data() };
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
        CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        error = L"AIサービス設定を復号できません。Windowsユーザーが変わった可能性があります。";
        return false;
    }

    const bool decoded = AIProviderSettingsCodec::Decode(
        output.pbData, output.cbData, settings);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    SecureZeroMemory(encrypted.data(), encrypted.size());

    if (!decoded)
    {
        settings = AIProviderSettings{};
        error = L"AIサービス設定の形式またはバージョンが正しくありません。";
        return false;
    }

    // v0.85.0 and later use direct provider connections only. Normalize
    // legacy Gateway settings without discarding provider keys or models.
    settings.route = AIConnectionRoute::Direct;
    settings.cloudflareAccountId.clear();
    settings.cloudflareGatewayId.clear();
    if (!settings.cloudflareApiToken.empty())
        SecureZeroMemory(settings.cloudflareApiToken.data(),
            settings.cloudflareApiToken.size() * sizeof(wchar_t));
    settings.cloudflareApiToken.clear();
    settings.gatewayRequestLogging = false;
    settings.gatewayCacheEnabled = false;
    return true;
}

bool SaveAIProviderSettings(const AIProviderSettings& settings, std::wstring& error)
{
    error.clear();
    AIProviderSettings normalized = settings;
    normalized.route = AIConnectionRoute::Direct;
    normalized.cloudflareAccountId.clear();
    normalized.cloudflareGatewayId.clear();
    if (!normalized.cloudflareApiToken.empty())
        SecureZeroMemory(normalized.cloudflareApiToken.data(),
            normalized.cloudflareApiToken.size() * sizeof(wchar_t));
    normalized.cloudflareApiToken.clear();
    normalized.gatewayRequestLogging = false;
    normalized.gatewayCacheEnabled = false;

    std::vector<std::uint8_t> plain;
    const bool encoded = AIProviderSettingsCodec::Encode(normalized, plain);
    for (auto* secret : { &normalized.geminiApiKey, &normalized.groqApiKey, &normalized.workersApiToken })
        if (!secret->empty()) SecureZeroMemory(secret->data(), secret->size() * sizeof(wchar_t));
    if (!encoded)
    {
        ClearBytes(plain);
        error = L"AIサービス設定に不正な値が含まれています（文字数または選択値を確認してください）。";
        return false;
    }

    DATA_BLOB input{ static_cast<DWORD>(plain.size()), plain.data() };
    DATA_BLOB encrypted{};
    if (!CryptProtectData(&input, L"Becky AI Assistant provider settings",
        nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted))
    {
        ClearBytes(plain);
        error = L"Windows DPAPIによるAIサービス設定の暗号化に失敗しました。";
        return false;
    }
    ClearBytes(plain);

    std::wstring path;
    if (!GetProviderStorePath(path, true))
    {
        SecureZeroMemory(encrypted.pbData, encrypted.cbData);
        LocalFree(encrypted.pbData);
        error = L"AIサービス設定の保存先を作成できません。";
        return false;
    }

    const std::wstring tempPath = path + L".tmp";
    std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
    const bool written = file && static_cast<bool>(file.write(
        reinterpret_cast<const char*>(encrypted.pbData), encrypted.cbData));
    file.close();
    SecureZeroMemory(encrypted.pbData, encrypted.cbData);
    LocalFree(encrypted.pbData);

    if (!written || !MoveFileExW(tempPath.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(tempPath.c_str());
        error = L"暗号化したAIサービス設定を保存できません。";
        return false;
    }

    // Keep the legacy Gemini key file synchronized while the existing Gemini
    // request path still reads it. This preserves current behavior during migration.
    if (!settings.geminiApiKey.empty() &&
        !SaveGeminiApiKey(settings.geminiApiKey, error))
        return false;

    return true;
}
