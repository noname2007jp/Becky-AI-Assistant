#include "GroqClient.h"

#include <windows.h>
#include <winhttp.h>
#include <string>

#include "gemini/GeminiError.h"

#pragma comment(lib, "winhttp.lib")

namespace
{
std::string Utf8(const std::wstring& value)
{
    if (value.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), count, nullptr, nullptr);
    return result;
}

std::wstring Wide(const std::string& value)
{
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), count);
    return result;
}

std::string EscapeJson(const std::string& value)
{
    std::string result;
    result.reserve(value.size() + 16);
    for (unsigned char c : value)
    {
        switch (c)
        {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (c < 0x20) result += ' ';
            else result += static_cast<char>(c);
        }
    }
    return result;
}

bool ReadJsonString(const std::string& json, size_t& position, std::string& value)
{
    if (position >= json.size() || json[position++] != '"') return false;
    value.clear();
    while (position < json.size())
    {
        const unsigned char c = static_cast<unsigned char>(json[position++]);
        if (c == '"') return true;
        if (c < 0x20) return false;
        if (c != '\\')
        {
            value.push_back(static_cast<char>(c));
            continue;
        }
        if (position >= json.size()) return false;
        const char escaped = json[position++];
        switch (escaped)
        {
        case '"': value.push_back('"'); break;
        case '\\': value.push_back('\\'); break;
        case '/': value.push_back('/'); break;
        case 'b': value.push_back('\b'); break;
        case 'f': value.push_back('\f'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case 'u':
        {
            if (position + 4 > json.size()) return false;
            unsigned int cp = 0;
            for (int i = 0; i < 4; ++i)
            {
                const char h = json[position++];
                cp <<= 4;
                if (h >= '0' && h <= '9') cp |= static_cast<unsigned int>(h - '0');
                else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned int>(h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned int>(h - 'A' + 10);
                else return false;
            }
            // Reject isolated surrogate escapes instead of producing invalid UTF-8.
            if (cp >= 0xd800 && cp <= 0xdfff) return false;
            if (cp <= 0x7f) value.push_back(static_cast<char>(cp));
            else if (cp <= 0x7ff) {
                value.push_back(static_cast<char>(0xc0 | (cp >> 6)));
                value.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            } else {
                value.push_back(static_cast<char>(0xe0 | (cp >> 12)));
                value.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                value.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            }
            break;
        }
        default: return false;
        }
    }
    return false;
}

bool ExtractStringProperty(const std::string& json, const char* property, std::wstring& output)
{
    output.clear();
    size_t position = 0;
    while (position < json.size())
    {
        if (json[position] != '"') { ++position; continue; }
        std::string key;
        if (!ReadJsonString(json, position, key)) return false;
        size_t valuePosition = position;
        while (valuePosition < json.size() &&
            (json[valuePosition] == ' ' || json[valuePosition] == '\t' ||
             json[valuePosition] == '\r' || json[valuePosition] == '\n')) ++valuePosition;
        if (valuePosition >= json.size() || json[valuePosition] != ':') continue;
        ++valuePosition;
        while (valuePosition < json.size() &&
            (json[valuePosition] == ' ' || json[valuePosition] == '\t' ||
             json[valuePosition] == '\r' || json[valuePosition] == '\n')) ++valuePosition;
        if (key != property || valuePosition >= json.size() || json[valuePosition] != '"')
        {
            position = valuePosition;
            continue;
        }
        std::string utf8;
        if (!ReadJsonString(json, valuePosition, utf8) || utf8.empty()) return false;
        output = Wide(utf8);
        return !output.empty();
    }
    return false;
}

std::wstring HttpError(unsigned long status)
{
    if (status == 400) return L"Groq APIがリクエストを受け付けませんでした。モデルIDや入力内容を確認してください。";
    if (status == 401) return L"Groq APIキーが無効です。Groq ConsoleでAPIキーを確認してください。";
    if (status == 403) return L"Groq APIへのアクセスが拒否されました。アカウントとモデルの利用権限を確認してください。";
    if (status == 404) return L"Groq APIで指定したモデルまたはエンドポイントが見つかりません。モデルIDを確認してください。";
    if (status == 429) return L"Groq APIのレート制限または無料利用枠に達しました。時間をおいて再試行してください。";
    if (status >= 500) return L"Groq API側で一時的な障害が発生しています。時間をおいて再試行してください。";
    return L"Groq APIから予期しないHTTP応答がありました。";
}
}

GeminiGenerateResult GenerateGroqChatCompletion(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& systemPrompt,
    const std::wstring& userPrompt,
    unsigned int maxOutputTokens,
    const std::atomic<int>* cancelState)
{
    GeminiGenerateResult result{ false, 0, {}, L"" };
    if (apiKey.empty()) { result.error = L"Groq APIキーが設定されていません。"; return result; }
    if (modelId.empty()) { result.error = L"GroqモデルIDが設定されていません。"; return result; }
    if (userPrompt.empty()) { result.error = L"AIへ送信する内容がありません。"; return result; }
    if (cancelState && cancelState->load(std::memory_order_acquire) != 0) {
        result.cancelled = true;
        return result;
    }

    const std::string payload =
        "{\"model\":\"" + EscapeJson(Utf8(modelId)) +
        "\",\"messages\":[{\"role\":\"system\",\"content\":\"" +
        EscapeJson(Utf8(systemPrompt)) + "\"},{\"role\":\"user\",\"content\":\"" +
        EscapeJson(Utf8(userPrompt)) + "\"}],\"max_tokens\":" +
        std::to_string(maxOutputTokens) + "}";

    HINTERNET session = WinHttpOpen(L"Becky-AI-Assistant/0.75.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { result.error = L"Groq APIへの通信セッションを開始できません。"; return result; }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 30000);
    HINTERNET connection = WinHttpConnect(session, L"api.groq.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"POST",
        L"/openai/v1/chat/completions", nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (!request)
    {
        if (connection) WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        result.error = L"Groq APIへの接続を開始できません。";
        return result;
    }

    std::wstring headers = L"Authorization: Bearer " + apiKey +
        L"\r\nContent-Type: application/json; charset=utf-8\r\nAccept: application/json\r\n";
    BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(headers.size()),
        const_cast<char*>(payload.data()), static_cast<DWORD>(payload.size()),
        static_cast<DWORD>(payload.size()), 0);
    if (!headers.empty()) SecureZeroMemory(headers.data(), headers.size() * sizeof(wchar_t));
    headers.clear();

    BOOL received = sent ? WinHttpReceiveResponse(request, nullptr) : FALSE;
    if (!received)
    {
        const DWORD error = GetLastError();
        result.error = FormatWinHttpError(error);
        result.retryable = IsRetryableWinHttpError(error);
    }
    else
    {
        DWORD status = 0, statusSize = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
        result.httpStatus = status;

        std::string response;
        DWORD available = 0;
        bool readFailed = false, tooLarge = false;
        DWORD readError = ERROR_SUCCESS;
        for (;;)
        {
            if (cancelState && cancelState->load(std::memory_order_acquire) != 0) {
                result.cancelled = true;
                break;
            }
            if (!WinHttpQueryDataAvailable(request, &available)) {
                readFailed = true; readError = GetLastError(); break;
            }
            if (!available) break;
            if (response.size() + available > 2 * 1024 * 1024) { tooLarge = true; break; }
            const size_t oldSize = response.size();
            response.resize(oldSize + available);
            DWORD read = 0;
            if (!WinHttpReadData(request, response.data() + oldSize, available, &read)) {
                readFailed = true; readError = GetLastError(); response.resize(oldSize); break;
            }
            response.resize(oldSize + read);
            if (!read) break;
        }

        if (!result.cancelled)
        {
            if (status == 200)
            {
                if (tooLarge) result.error = L"Groq APIの応答データが上限（2MB）を超えました。";
                else if (readFailed) {
                    result.error = FormatWinHttpError(readError);
                    result.retryable = IsRetryableWinHttpError(readError);
                }
                else if (!ExtractStringProperty(response, "content", result.text)) {
                    result.error = L"Groq APIの応答から文章を取得できませんでした。";
                    result.retryable = true;
                }
                else result.success = true;
            }
            else
            {
                result.error = HttpError(status);
                result.retryable = status == 408 || status == 429 || status >= 500;
                std::wstring providerMessage;
                if (ExtractStringProperty(response, "message", providerMessage) && !providerMessage.empty())
                    result.error += L"\n詳細: " + providerMessage;
            }
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return result;
}
