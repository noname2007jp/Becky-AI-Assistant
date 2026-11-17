#include "GeminiReply.h"
#include <windows.h>
#include <winhttp.h>
#include "GeminiError.h"
#include "PiiMasker.h"
#include "GeminiDebugLog.h"
#include <string>

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
std::string JsonEscape(const std::string& s)
{
    std::string out;
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: if (c < 0x20) out += ' '; else out += (char)c;
        }
    }
    return out;
}
bool ExtractText(const std::string& json, std::wstring& output)
{
    size_t p = json.find("\"text\"");
    if (p == std::string::npos) return false;
    p = json.find(':', p);
    p = json.find('"', p);
    if (p == std::string::npos) return false;
    ++p;
    std::string value;
    bool escape = false;
    for (; p < json.size(); ++p)
    {
        char c = json[p];
        if (!escape && c == '"') break;
        if (!escape && c == '\\') { escape = true; continue; }
        if (escape)
        {
            switch (c)
            {
            case '"': value += '"'; break;
            case '\\': value += '\\'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            default: value += c; break;
            }
            escape = false;
        }
        else value += c;
    }
    if (p >= json.size() || escape) return false;
    output = Wide(value);
    return !output.empty();
}
}

std::wstring BuildMailReplyPrompt(
    const std::wstring& toneInstruction,
    const std::wstring& subject,
    const std::wstring& from,
    const std::wstring& body,
    const std::wstring& consultationTranscript)
{
    std::wstring prompt = BuildMailReplyPrompt(
        toneInstruction, subject, from, body, consultationTranscript);

    PiiMasker localMasker;
    PiiMasker& masker = externalMasker ? *externalMasker : localMasker;
    if (!masker.IsReady()) { result.error = masker.Error(); return result; }
    prompt = masker.Mask(prompt);
    std::string generationConfig = "\"maxOutputTokens\":2048";
    if (modelId.find(L"gemini-2.5-") == 0)
    {
        int budget = (thinkingLevel == L"OFF") ? 0 : -1;
        generationConfig += ",\"thinkingConfig\":{\"thinkingBudget\":" + std::to_string(budget) + "}";
    }
    else if (modelId.find(L"gemma-") != 0 && !thinkingLevel.empty())
        generationConfig += ",\"thinkingConfig\":{\"thinkingLevel\":\"" + Utf8(thinkingLevel) + "\"}";

    const std::string payload =
        "{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"" +
        JsonEscape(Utf8(prompt)) + "\"}]}],\"generationConfig\":{" + generationConfig + "}}";
    LogGeminiRequest(L"reply", modelId, masker.IsEnabled(), masker.ReplacementCount(), payload);

    HINTERNET session = WinHttpOpen(L"Becky-AI-Assistant/0.14",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { result.error = L"通信セッションを開始できません。"; return result; }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 30000);
    HINTERNET connection = WinHttpConnect(session, L"generativelanguage.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    const std::wstring path = L"/v1beta/models/" + modelId + L":generateContent";
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"POST", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (!request)
    {
        if (connection) WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        result.error = L"Gemini APIへの接続を開始できません。";
        return result;
    }
    const std::wstring headers = L"x-goog-api-key: " + apiKey + L"\r\nContent-Type: application/json; charset=utf-8\r\n";
    BOOL sent = WinHttpSendRequest(request, headers.c_str(), (DWORD)headers.size(),
        (LPVOID)payload.data(), (DWORD)payload.size(), (DWORD)payload.size(), 0);
    BOOL received = sent ? WinHttpReceiveResponse(request, nullptr) : FALSE;
    if (!received) {
        const DWORD networkError = GetLastError();
        result.error = FormatWinHttpError(networkError);
        result.retryable = networkError == ERROR_WINHTTP_TIMEOUT;
    }
    else
    {
        DWORD code = 0, size = sizeof(code);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX);
        result.httpStatus = code;
        std::string response;
        DWORD available = 0;
        bool responseReadFailed = false;
        DWORD responseReadError = ERROR_SUCCESS;
        for (;;)
        {
            if (!WinHttpQueryDataAvailable(request, &available))
            {
                responseReadFailed = true;
                responseReadError = GetLastError();
                break;
            }
            if (!available) break;
            if (response.size() + available > 2 * 1024 * 1024) break;
            size_t old = response.size();
            response.resize(old + available);
            DWORD read = 0;
            if (!WinHttpReadData(request, response.data() + old, available, &read))
            {
                responseReadFailed = true;
                responseReadError = GetLastError();
                break;
            }
            response.resize(old + read);
            if (!read) break;
        }
        if (code == 200)
        {
            if (responseReadFailed)
            {
                result.error = FormatWinHttpError(responseReadError);
                result.retryable = responseReadError == ERROR_WINHTTP_TIMEOUT;
            }
            else
            {
                result.success = ExtractText(response, result.text);
                if (result.success) result.text = masker.Restore(result.text);
                else { result.error = L"応答から返信文案を取得できませんでした。"; result.retryable = true; }
            }
        }
        else {
            result.error = FormatGeminiHttpError(code);
            result.retryable = code == 404 || code == 408 || code == 429 || (code >= 500 && code <= 599);
        }
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    LogGeminiResponse(L"reply", modelId, result.httpStatus, result.success, result.error);
    return result;
}
