#include "GeminiModels.h"
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <algorithm>
#include <fstream>
#include <set>

#pragma comment(lib, "winhttp.lib")

namespace
{
std::string Utf8(const std::wstring& value)
{
    if (value.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), out.data(), n, nullptr, nullptr);
    return out;
}
std::wstring Wide(const std::string& value)
{
    if (value.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), out.data(), n);
    return out;
}
std::wstring CatalogPath()
{
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK) return {};
    const std::wstring dir = std::wstring(appData) + L"\\Becky-AI-Assistant";
    if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return {};
    return dir + L"\\models-catalog.txt";
}
bool ReadJsonString(const std::string& json, size_t& pos, std::string& value)
{
    if (pos >= json.size() || json[pos] != '"') return false;
    ++pos; value.clear(); bool escaped = false;
    for (; pos < json.size(); ++pos)
    {
        const char c = json[pos];
        if (!escaped && c == '"') { ++pos; return true; }
        if (!escaped && c == '\\') { escaped = true; continue; }
        if (escaped)
        {
            switch (c) { case 'n': value += '\n'; break; case 'r': value += '\r'; break;
            case 't': value += '\t'; break; case '"': value += '"'; break; case '\\': value += '\\'; break;
            default: value += c; break; }
            escaped = false;
        }
        else value += c;
    }
    return false;
}
std::string StringField(const std::string& object, const char* field)
{
    const std::string key = std::string("\"") + field + "\"";
    size_t p = object.find(key);
    if (p == std::string::npos) return {};
    p = object.find(':', p + key.size());
    if (p == std::string::npos) return {};
    p = object.find('"', p + 1);
    if (p == std::string::npos) return {};
    std::string value;
    return ReadJsonString(object, p, value) ? value : std::string{};
}
bool HasGenerateContent(const std::string& object)
{
    const size_t p = object.find("\"supportedGenerationMethods\"");
    if (p == std::string::npos) return false;
    const size_t begin = object.find('[', p);
    const size_t end = begin == std::string::npos ? std::string::npos : object.find(']', begin);
    return begin != std::string::npos && end != std::string::npos &&
        object.find("\"generateContent\"", begin) < end;
}
bool SupportsThinking(const std::string& object)
{
    const size_t p = object.find("\"thinking\"");
    if (p == std::string::npos) return false;
    const size_t colon = object.find(':', p);
    if (colon == std::string::npos) return false;
    return object.find("true", colon) < object.find_first_of(",}", colon);
}
bool IsTextModel(const std::string& id)
{
    if (id.rfind("gemini-", 0) != 0 && id.rfind("gemma-", 0) != 0) return false;
    static const char* excluded[] = { "-image", "-tts", "-live", "transcribe", "embedding",
        "omni", "veo", "lyria", "robotics", "deep-research", "computer-use", "decision" };
    for (const char* term : excluded) if (id.find(term) != std::string::npos) return false;
    return true;
}
bool IsFreeTierModel(const std::string& id)
{
    // Curated from Google's published pricing table; update this allowlist with the pricing docs.
    static const std::set<std::string> freeModels = {
        "gemini-4-hydrogen", "gemini-3.8-flash", "gemini-3.6-flash",
        "gemini-3.5-flash-lite"
    };
    return freeModels.find(id) != freeModels.end();
}
bool DownloadModelPage(const std::wstring& apiKey, const std::wstring& path, std::string& response, std::wstring& error)
{
    HINTERNET session = WinHttpOpen(L"Becky-AI-Assistant/0.29",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { error = L"モデル一覧の通信を開始できません。"; return false; }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 30000);
    HINTERNET connection = WinHttpConnect(session, L"generativelanguage.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (!request)
    {
        if (connection) WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session); error = L"モデル一覧APIへ接続できません。"; return false;
    }
    const std::wstring headers = L"x-goog-api-key: " + apiKey + L"\r\nAccept: application/json\r\n";
    BOOL sent = WinHttpSendRequest(request, headers.c_str(), (DWORD)headers.size(),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    BOOL received = sent ? WinHttpReceiveResponse(request, nullptr) : FALSE;
    if (!received)
    {
        const DWORD code = GetLastError();
        WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
        error = L"モデル一覧を取得できませんでした（通信エラー " + std::to_wstring(code) + L"）。";
        return false;
    }
    DWORD status = 0, statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available)
    {
        if (response.size() + available > 2 * 1024 * 1024) break;
        const size_t old = response.size(); response.resize(old + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, response.data() + old, available, &read)) { response.resize(old); break; }
        response.resize(old + read); if (!read) break;
    }
    WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    if (status != 200)
    {
        error = L"モデル一覧APIがHTTP " + std::to_wstring(status) + L"を返しました。APIキーと利用権限を確認してください。";
        return false;
    }
    return true;
}
void ParseModels(const std::string& json, std::vector<GeminiModelOption>& models, bool includePaid)
{
    const size_t modelsKey = json.find("\"models\"");
    if (modelsKey == std::string::npos) return;
    size_t pos = json.find('[', modelsKey);
    if (pos == std::string::npos) return;
    ++pos;
    while (pos < json.size())
    {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\r' || json[pos] == '\n' || json[pos] == '\t' || json[pos] == ',')) ++pos;
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] != '{') { ++pos; continue; }
        const size_t begin = pos; int depth = 0; bool inString = false; bool escape = false;
        for (; pos < json.size(); ++pos)
        {
            const char c = json[pos];
            if (inString)
            {
                if (!escape && c == '"') inString = false;
                if (!escape && c == '\\') escape = true; else escape = false;
                continue;
            }
            if (c == '"') { inString = true; continue; }
            if (c == '{') ++depth;
            else if (c == '}' && --depth == 0) { ++pos; break; }
        }
        const std::string object = json.substr(begin, pos - begin);
        if (!HasGenerateContent(object)) continue;
        std::string name = StringField(object, "name");
        if (name.rfind("models/", 0) == 0) name.erase(0, 7);
        if (!IsTextModel(name)) continue;
        const bool freeTier = IsFreeTierModel(name);
        if (!includePaid && !freeTier) continue;
        std::string display = StringField(object, "displayName");
        if (display.empty()) display = name;
        std::wstring thinking;
        if (SupportsThinking(object))
            thinking = name.rfind("gemini-2.5-", 0) == 0 ? L"ON" : L"MEDIUM";
        std::wstring label = Wide(display) + (freeTier ? L"（無料枠対象）" : L"（有料・料金要確認）");
        models.push_back({ label, Wide(name), thinking, freeTier });
    }
}
}

std::vector<GeminiModelOption> DefaultGeminiModels()
{
    return {
        { L"Gemini 4 Hydrogen（無料枠対象）", L"gemini-4-hydrogen", L"MEDIUM", true },
        { L"Gemini 3.8 Flash（無料枠対象）", L"gemini-3.8-flash", L"MEDIUM", true },
        { L"Gemini 3.6 Flash（無料枠対象）", L"gemini-3.6-flash", L"MEDIUM", true },
        { L"Gemini 3.5 Flash-Lite（無料枠対象）", L"gemini-3.5-flash-lite", L"MEDIUM", true }
    };
}

bool LoadGeminiModelCatalog(std::vector<GeminiModelOption>& models, std::wstring& error)
{
    models.clear(); error.clear();
    const std::wstring path = CatalogPath();
    if (path.empty()) { error = L"モデル一覧の保存先を取得できません。"; return false; }
    std::ifstream file(path, std::ios::binary);
    if (!file) { models = DefaultGeminiModels(); return true; }
    std::string line;
    while (std::getline(file, line))
    {
        const size_t a = line.find('|'), b = a == std::string::npos ? a : line.find('|', a + 1);
        const size_t c = b == std::string::npos ? b : line.find('|', b + 1);
        if (a == std::string::npos || b == std::string::npos || c == std::string::npos) continue;
        const std::string id = line.substr(0, a);
        const std::string thinking = line.substr(a + 1, b - a - 1);
        const std::string label = line.substr(b + 1, c - b - 1);
        const bool freeTier = line.substr(c + 1) == "1";
        if (!id.empty()) models.push_back({ Wide(label), Wide(id), Wide(thinking), freeTier });
    }
    if (models.empty()) { models = DefaultGeminiModels(); }
    return true;
}

bool SaveGeminiModelCatalog(const std::vector<GeminiModelOption>& models, std::wstring& error)
{
    error.clear();
    const std::wstring path = CatalogPath();
    if (path.empty()) { error = L"モデル一覧の保存先を取得できません。"; return false; }
    const std::wstring temp = path + L".tmp";
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    if (!file) { error = L"モデル一覧ファイルを作成できません。"; return false; }
    for (const auto& model : models)
    {
        const std::string line = Utf8(model.id) + "|" + Utf8(model.thinking) + "|" +
            Utf8(model.label) + "|" + (model.freeTier ? "1" : "0") + "\n";
        file.write(line.data(), static_cast<std::streamsize>(line.size()));
    }
    file.close();
    if (!file || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temp.c_str()); error = L"モデル一覧を保存できません。"; return false;
    }
    return true;
}

bool FetchGeminiModelCatalog(const std::wstring& apiKey, bool includePaid,
    std::vector<GeminiModelOption>& models, std::wstring& error)
{
    models.clear(); error.clear();
    if (apiKey.empty()) { error = L"Gemini APIキーが設定されていません。"; return false; }
    std::string response;
    if (!DownloadModelPage(apiKey, L"/v1beta/models?pageSize=1000", response, error)) return false;
    ParseModels(response, models, includePaid);
    std::sort(models.begin(), models.end(), [](const auto& a, const auto& b) {
        if (a.freeTier != b.freeTier) return a.freeTier > b.freeTier;
        return a.label < b.label;
    });
    if (models.empty())
    {
        error = includePaid
            ? L"generateContent対応のテキスト生成モデルが見つかりませんでした。"
            : L"無料枠対象モデルが見つかりませんでした。料金区分またはAPIキーの利用可能モデルを確認してください。";
        return false;
    }
    return true;
}
