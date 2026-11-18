#include "GeminiModels.h"
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <algorithm>
#include <fstream>
#include <set>
#include <cctype>
#include <cstring>
#include <utility>

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
std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
std::string StripHtml(std::string value)
{
    std::string out; bool inTag = false;
    for (char c : value)
    {
        if (c == '<') { inTag = true; out += ' '; }
        else if (c == '>') { inTag = false; out += ' '; }
        else if (!inTag) out += c;
    }
    const std::pair<const char*, const char*> entities[] = {
        {"&nbsp;", " "}, {"&amp;", "&"}, {"&quot;", "\""}, {"&#39;", "'"},
        {"&lt;", "<"}, {"&gt;", ">"}
    };
    for (const auto& e : entities)
    {
        size_t p = 0;
        while ((p = out.find(e.first, p)) != std::string::npos)
        { out.replace(p, std::strlen(e.first), e.second); p += std::strlen(e.second); }
    }
    return out;
}
bool ReadFreeTierFromSection(const std::string& page, const std::string& modelId,
    bool& known, bool& freeTier)
{
    known = false; freeTier = false;
    // Google documents the Standard table as rows: Input price / Free Tier value.
    const std::string anchor = "id=\"" + modelId + "\"";
    size_t modelStart = page.find(anchor);
    if (modelStart == std::string::npos) return false;

    size_t modelEnd = page.size();
    const char* boundaries[] = { "## Gemini ", "## Gemma ", "{% id=\"gemini-", "{% id=\"gemma-" };
    for (const char* boundary : boundaries)
    {
        size_t next = page.find(boundary, modelStart + anchor.size());
        if (next != std::string::npos && next < modelEnd) modelEnd = next;
    }
    std::string section = Lower(StripHtml(page.substr(modelStart, modelEnd - modelStart)));
    size_t standardStart = section.find("title=\"standard\"");
    if (standardStart == std::string::npos) standardStart = section.find("title='standard'");
    if (standardStart == std::string::npos) return false;
    size_t standardEnd = section.find("{% /tab %}", standardStart);
    if (standardEnd == std::string::npos) standardEnd = section.size();
    const std::string standard = section.substr(standardStart, standardEnd - standardStart);

    size_t input = standard.find("input price");
    if (input == std::string::npos) return false;
    size_t output = standard.find("output price", input + 1);
    if (output == std::string::npos) output = standard.size();
    const std::string inputRow = standard.substr(input, output - input);

    size_t newline = inputRow.find('\n');
    std::string value = newline == std::string::npos ? inputRow : inputRow.substr(newline + 1);
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' ||
        value.front() == '*' || value.front() == '-' || value.front() == '\r'))
        value.erase(value.begin());
    value = Lower(value);
    if (value.find("free of charge") == 0 || value.find("free") == 0 || value.find("無料") == 0)
    { known = true; freeTier = true; return true; }
    if (value.find("not available") == 0 || value.find("利用不可") == 0 ||
        value.find('$') == 0 || value.find("€") == 0)
    { known = true; freeTier = false; return true; }

    // Fallback for HTML-rendered tables: only inspect the Input price row.
    if (inputRow.find("free of charge") != std::string::npos || inputRow.find("無料") != std::string::npos)
    { known = true; freeTier = true; return true; }
    if (inputRow.find("not available") != std::string::npos || inputRow.find("利用不可") != std::string::npos ||
        inputRow.find('$') != std::string::npos || inputRow.find("€") != std::string::npos)
    { known = true; freeTier = false; return true; }
    return false;
}
bool DownloadPricingPage(std::string& response, std::wstring& error)
{
    HINTERNET session = WinHttpOpen(L"Becky-AI-Assistant/0.65.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { error = L"Google公式料金ページへの通信を開始できません。"; return false; }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 30000);
    HINTERNET connection = WinHttpConnect(session, L"ai.google.dev", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET",
        L"/gemini-api/docs/pricing", nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (!request)
    {
        if (connection) WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session); error = L"Google公式料金ページへ接続できません。"; return false;
    }
    const wchar_t* headers = L"Accept: text/markdown, text/plain;q=0.9, text/html;q=0.8, */*;q=0.7\r\n";
    BOOL sent = WinHttpSendRequest(request, headers, (DWORD)-1L,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    BOOL received = sent ? WinHttpReceiveResponse(request, nullptr) : FALSE;
    if (!received)
    {
        const DWORD code = GetLastError();
        WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
        error = L"Google公式料金ページを取得できませんでした（通信エラー " + std::to_wstring(code) + L"）。";
        return false;
    }
    DWORD status = 0, statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available)
    {
        if (response.size() + available > 8 * 1024 * 1024) break;
        const size_t old = response.size(); response.resize(old + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, response.data() + old, available, &read))
        { response.resize(old); break; }
        response.resize(old + read); if (!read) break;
    }
    WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    if (status != 200 || response.empty())
    {
        error = L"Google公式料金ページがHTTP " + std::to_wstring(status) +
            L"を返しました。料金情報を確認できないため、モデル一覧は更新しませんでした。";
        return false;
    }
    return true;
}
bool DownloadModelPage(const std::wstring& apiKey, const std::wstring& path, std::string& response, std::wstring& error)
{
    HINTERNET session = WinHttpOpen(L"Becky-AI-Assistant/0.65.0",
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
void ParseModels(const std::string& json, const std::string& pricingPage,
    std::vector<GeminiModelOption>& models, bool includePaid)
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
        bool pricingKnown = false, freeTier = false;
        ReadFreeTierFromSection(pricingPage, name, pricingKnown, freeTier);
        if (!includePaid && (!pricingKnown || !freeTier)) continue;
        std::string display = StringField(object, "displayName");
        if (display.empty()) display = name;
        std::wstring thinking;
        if (SupportsThinking(object))
            thinking = name.rfind("gemini-2.5-", 0) == 0 ? L"ON" : L"MEDIUM";
        std::wstring label = Wide(display) + (freeTier ? L"（無料枠あり）" :
            (pricingKnown ? L"（無料枠なし）" : L"（料金未確認）"));
        models.push_back({ label, Wide(name), thinking, freeTier, pricingKnown });
    }
}
}

std::vector<GeminiModelOption> DefaultGeminiModels()
{
    // Before the first pricing refresh, expose only conservative defaults as unclassified.
    return {
        { L"Gemini 3.5 Flash-Lite（料金未確認）", L"gemini-3.5-flash-lite", L"MEDIUM", false, false },
        { L"Gemini 3.8 Flash（料金未確認）", L"gemini-3.8-flash", L"MEDIUM", false, false },
        { L"Gemini 3.6 Flash（料金未確認）", L"gemini-3.6-flash", L"MEDIUM", false, false }
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
        const size_t d = line.find('|', c + 1);
        const bool pricingKnown = d != std::string::npos && line.substr(d + 1) == "1";
        const bool freeTier = pricingKnown &&
            line.substr(c + 1, d == std::string::npos ? d : d - c - 1) == "1";
        std::string safeLabel = label;
        if (!pricingKnown)
        {
            const std::string stale = "（無料枠対象）";
            const size_t stalePos = safeLabel.find(stale);
            if (stalePos != std::string::npos) safeLabel.replace(stalePos, stale.size(), "（料金未確認）");
        }
        if (!id.empty()) models.push_back({ Wide(safeLabel), Wide(id), Wide(thinking), freeTier, pricingKnown });
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
            Utf8(model.label) + "|" + (model.freeTier ? "1" : "0") + "|" + (model.pricingKnown ? "1" : "0") + "\n";
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
    std::string response, pricingPage;
    if (!DownloadPricingPage(pricingPage, error)) return false;
    if (!DownloadModelPage(apiKey, L"/v1beta/models?pageSize=1000", response, error)) return false;
    ParseModels(response, pricingPage, models, includePaid);
    std::sort(models.begin(), models.end(), [](const auto& a, const auto& b) {
        if (a.freeTier != b.freeTier) return a.freeTier > b.freeTier;
        return a.label < b.label;
    });
    if (models.empty())
    {
        error = includePaid
            ? L"generateContent対応のテキスト生成モデルが見つかりませんでした。"
            : L"Google公式料金ページでStandardの無料枠を確認できるモデルがありませんでした。料金情報を取得できない場合は、有料モデルも含める設定で再試行してください。";
        return false;
    }
    return true;
}
