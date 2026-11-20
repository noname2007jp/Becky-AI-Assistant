#include "ModelCatalogPolicy.h"

#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace
{
std::vector<ModelCatalogPolicyEntry> g_entries;
bool g_loaded = false;

std::wstring CachePath(bool createDirectory)
{
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | (createDirectory ? CSIDL_FLAG_CREATE : 0),
        nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK) return {};
    const std::wstring dir = std::wstring(appData) + L"\\Becky-AI-Assistant";
    if (createDirectory && !CreateDirectoryW(dir.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) return {};
    return dir + L"\\model-catalog-policy.json";
}

std::wstring Wide(const std::string& value)
{
    if (value.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), out.data(), n) != n) return {};
    return out;
}

std::string Utf8(const std::wstring& value)
{
    if (value.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), out.data(), n, nullptr, nullptr) != n) return {};
    return out;
}

std::string JsonString(const std::string& object, const char* name)
{
    const std::regex pattern(std::string("\\"") + name +
        "\\"\\s*:\\s*\\"((?:\\\\.|[^\\"\\\\])*)\\"");
    std::smatch match;
    if (!std::regex_search(object, match, pattern)) return {};
    std::string value = match[1].str();
    std::string decoded;
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '\\\\' && i + 1 < value.size())
        {
            const char c = value[++i];
            if (c == 'n' || c == 'r' || c == 't') decoded.push_back(' ');
            else decoded.push_back(c);
        }
        else decoded.push_back(value[i]);
    }
    return decoded;
}

bool ParsePolicy(const std::string& json, std::vector<ModelCatalogPolicyEntry>& entries)
{
    if (json.size() > 256 * 1024 || json.find("\\"schema_version\\"\\s*:\\s*1") != std::string::npos)
    {
        // Schema validation below is intentionally explicit; the string search above
        // only rejects oversized data and does not determine the schema version.
    }
    if (json.size() > 256 * 1024 ||
        !std::regex_search(json, std::regex("\\"schema_version\\"\\s*:\\s*1")))
        return false;

    const char* providers[] = { "gemini", "groq", "workers_ai" };
    std::vector<ModelCatalogPolicyEntry> parsed;
    for (int providerIndex = 0; providerIndex < 3; ++providerIndex)
    {
        const char* provider = providers[providerIndex];
    {
        const std::regex arrayPattern(std::string("\\"") + provider + "\\"\\s*:\\s*\\[");
        std::smatch arrayMatch;
        if (!std::regex_search(json, arrayMatch, arrayPattern)) return false;
        size_t pos = static_cast<size_t>(arrayMatch.position() + arrayMatch.length());
        int depth = 0;
        bool inString = false, escaped = false;
        size_t objectStart = std::string::npos;
        while (pos < json.size())
        {
            const char c = json[pos];
            if (inString)
            {
                if (!escaped && c == '\\"') inString = false;
                if (!escaped && c == '\\\\') escaped = true; else escaped = false;
                ++pos; continue;
            }
            if (c == '\\"') { inString = true; ++pos; continue; }
            if (c == '{') { if (depth++ == 0) objectStart = pos; }
            else if (c == '}')
            {
                if (--depth < 0) return false;
                if (depth == 0 && objectStart != std::string::npos)
                {
                    const std::string object = json.substr(objectStart, pos - objectStart + 1);
                    ModelCatalogPolicyEntry entry;
                    entry.provider = providerIndex;
                    entry.modelId = Wide(JsonString(object, "id"));
                    entry.status = Wide(JsonString(object, "status"));
                    entry.quality = Wide(JsonString(object, "quality"));
                    entry.reason = Wide(JsonString(object, "reason"));
                    entry.replacementModelId = Wide(JsonString(object, "replacement_model_id"));
                    if (entry.modelId.empty() || entry.modelId.size() > 512) return false;
                    if (entry.status != L"recommended" && entry.status != L"allowed" &&
                        entry.status != L"caution" && entry.status != L"blocked") return false;
                    if (entry.quality != L"unverified" && entry.quality != L"tested" &&
                        entry.quality != L"poor") return false;
                    if (std::any_of(parsed.begin(), parsed.end(), [&](const auto& x) {
                        return x.provider == entry.provider && x.modelId == entry.modelId;
                    })) return false;
                    parsed.push_back(std::move(entry));
                    objectStart = std::string::npos;
                }
            }
            else if (c == ']' && depth == 0) break;
            ++pos;
        }
        if (depth != 0) return false;
    }
    entries.swap(parsed);
    return true;
}

bool DownloadPolicy(std::string& body, std::wstring& error)
{
    HINTERNET session = WinHttpOpen(L"Becky-AI-Assistant/0.81.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { error = L"モデル評価情報の通信を開始できません。"; return false; }
    WinHttpSetTimeouts(session, 5000, 5000, 7000, 10000);
    HINTERNET connection = WinHttpConnect(session, L"raw.githubusercontent.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET",
        L"/noname2007jp/Becky-AI-Assistant/refs/heads/main/config/model-catalog-policy.json",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (!request)
    {
        if (connection) WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        error = L"モデル評価情報の取得先へ接続できません。";
        return false;
    }
    BOOL ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr);
    DWORD status = 0, size = sizeof(status);
    if (ok) WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    DWORD available = 0;
    while (ok && WinHttpQueryDataAvailable(request, &available) && available)
    {
        if (body.size() + available > 256 * 1024) { ok = FALSE; break; }
        const size_t old = body.size();
        body.resize(old + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, body.data() + old, available, &read)) { ok = FALSE; break; }
        body.resize(old + read);
        if (read == 0) break;
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (!ok || status != 200)
    {
        error = L"モデル評価情報を取得できませんでした";
        if (status) error += L"（HTTP " + std::to_wstring(status) + L"）";
        return false;
    }
    return true;
}

bool SaveCache(const std::string& json, std::wstring& error)
{
    const std::wstring path = CachePath(true);
    if (path.empty()) { error = L"モデル評価情報の保存先を作成できません。"; return false; }
    const std::wstring temp = path + L".tmp";
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    const bool written = file && static_cast<bool>(file.write(json.data(),
        static_cast<std::streamsize>(json.size())));
    file.close();
    if (!written || !MoveFileExW(temp.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temp.c_str());
        error = L"モデル評価情報を保存できません。";
        return false;
    }
    return true;
}

bool LoadFromBytes(const std::string& bytes, std::wstring& error)
{
    std::vector<ModelCatalogPolicyEntry> parsed;
    if (!ParsePolicy(bytes, parsed)) { error = L"モデル評価情報の形式またはスキーマが正しくありません。"; return false; }
    g_entries.swap(parsed);
    g_loaded = true;
    return true;
}
}

bool LoadModelCatalogPolicy(std::wstring& error)
{
    error.clear();
    const std::wstring path = CachePath(false);
    if (path.empty()) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return LoadFromBytes(bytes, error);
}

bool RefreshModelCatalogPolicy(std::wstring& error)
{
    error.clear();
    std::string bytes;
    if (!DownloadPolicy(bytes, error)) return false;
    std::vector<ModelCatalogPolicyEntry> parsed;
    if (!ParsePolicy(bytes, parsed))
    {
        error = L"取得したモデル評価情報の形式が正しくありません。保存済み情報を維持します。";
        return false;
    }
    if (!SaveCache(bytes, error)) return false;
    g_entries.swap(parsed);
    g_loaded = true;
    return true;
}

std::wstring GetModelCatalogPolicyNote(int provider, const std::wstring& modelId)
{
    if (!g_loaded) { std::wstring ignored; LoadModelCatalogPolicy(ignored); }
    const std::wstring id = modelId;
    for (const auto& entry : g_entries)
    {
        if (entry.provider != provider || entry.modelId != id) continue;
        std::wstring note = L"評価：";
        if (entry.status == L"recommended") note += L"推奨";
        else if (entry.status == L"allowed") note += L"利用可";
        else if (entry.status == L"caution") note += L"注意";
        else note += L"非推奨";
        note += L"／品質：";
        if (entry.quality == L"tested") note += L"確認済み";
        else if (entry.quality == L"poor") note += L"問題あり";
        else note += L"未評価";
        if (!entry.reason.empty()) note += L"\r\n" + entry.reason;
        if (!entry.replacementModelId.empty()) note += L"\r\n代替候補：" + entry.replacementModelId;
        return note;
    }
    return L"評価：未登録（品質未評価）";
}
