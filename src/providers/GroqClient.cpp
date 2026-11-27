#include "GroqClient.h"

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <cwchar>
#include <cwctype>

#include "gemini/GeminiError.h"
#include "gemini/GeminiTextContinuation.h"

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

bool IsSafeGatewayPart(const std::wstring& value, size_t maxLength)
{
    if (value.empty() || value.size() > maxLength) return false;
    for (wchar_t c : value)
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'-' || c == L'_')) return false;
    return true;
}

bool IsValidCloudflareAccountId(const std::wstring& value)
{
    if (value.size() != 32) return false;
    for (wchar_t c : value)
        if (!((c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F') ||
              (c >= L'0' && c <= L'9'))) return false;
    return true;
}

bool IsRateLimitError(const std::wstring& error)
{
    return error.find(L"TPM") != std::wstring::npos ||
        error.find(L"tokens per minute") != std::wstring::npos;
}

DWORD GetRateLimitRetryDelayMs(const std::wstring& error)
{
    const std::wstring marker = L"try again in ";
    const size_t pos = error.find(marker);
    if (pos == std::wstring::npos) return 15000;
    const wchar_t* begin = error.c_str() + pos + marker.size();
    wchar_t* end = nullptr;
    const double seconds = std::wcstod(begin, &end);
    if (end == begin || seconds <= 0.0) return 15000;
    // Add a 750 ms safety margin to the provider's suggested reset time.
    const double milliseconds = seconds * 1000.0 + 750.0;
    if (milliseconds > 60000.0) return 60000;
    return static_cast<DWORD>(milliseconds);
}

bool WaitForRateLimitRetry(DWORD delayMs, const std::atomic<int>* cancelState,
    const GroqProgressCallback& progress, int retryNumber)
{
    constexpr DWORD kPollIntervalMs = 100;
    DWORD elapsed = 0;
    unsigned int lastReportedSeconds = static_cast<unsigned int>(-1);
    while (elapsed < delayMs)
    {
        const unsigned int remainingSeconds =
            static_cast<unsigned int>((delayMs - elapsed + 999) / 1000);
        if (progress && remainingSeconds != lastReportedSeconds)
        {
            progress(L"TPM／OTPM回復待ち：あと" + std::to_wstring(remainingSeconds) +
                L"秒（再試行" + std::to_wstring(retryNumber) + L"/2）");
            lastReportedSeconds = remainingSeconds;
        }
        if (cancelState && cancelState->load(std::memory_order_acquire) != 0)
            return false;
        const DWORD slice = (delayMs - elapsed < kPollIntervalMs)
            ? delayMs - elapsed : kPollIntervalMs;
        Sleep(slice);
        elapsed += slice;
    }
    return !(cancelState && cancelState->load(std::memory_order_acquire) != 0);
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

static GeminiGenerateResult GenerateGroqChatCompletionOnce(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& systemPrompt,
    const std::wstring& userPrompt,
    const std::wstring& assistantContinuation,
    unsigned int maxOutputTokens,
    std::string* finishReason,
    const std::atomic<int>* cancelState,
    const AIProviderSettings* providerSettings)
{
    GeminiGenerateResult result{ false, 0, {}, L"" };
    if (apiKey.empty()) { result.error = L"Groq APIキーが設定されていません。"; return result; }
    if (modelId.empty()) { result.error = L"GroqモデルIDが設定されていません。"; return result; }
    if (userPrompt.empty()) { result.error = L"AIへ送信する内容がありません。"; return result; }
    if (cancelState && cancelState->load(std::memory_order_acquire) != 0) {
        result.cancelled = true;
        return result;
    }

    std::string messages =
        "[{\"role\":\"system\",\"content\":\"" +
        EscapeJson(Utf8(systemPrompt)) + "\"},{\"role\":\"user\",\"content\":\"" +
        EscapeJson(Utf8(userPrompt)) + "\"}";
    if (!assistantContinuation.empty())
    {
        messages += ",{\"role\":\"assistant\",\"content\":\"" +
            EscapeJson(Utf8(assistantContinuation)) + "\"}";
        messages += ",{\"role\":\"user\",\"content\":\"前の回答が出力上限で途中終了しました。"
            "直前の回答の続きだけを書いてください。すでに書いた内容を繰り返さず、"
            "説明の前置きや要約も追加しないでください。文の途中で切れている場合は、"
            "その位置から自然につなげてください。回答全体の構成・書式・文体を維持し、"
            "元の依頼への回答を最後まで完成させてください。\"}";
    }
    messages += "]";
    const std::string payload =
        "{\"model\":\"" + EscapeJson(Utf8(modelId)) +
        "\",\"messages\":" + messages + ",\"max_tokens\":" +
        std::to_string(maxOutputTokens) + "}";

    const bool useGateway = providerSettings &&
        providerSettings->route == AIConnectionRoute::CloudflareAIGateway;
    if (useGateway &&
        (!IsValidCloudflareAccountId(providerSettings->cloudflareAccountId) ||
         !IsSafeGatewayPart(providerSettings->cloudflareGatewayId, 64)))
    {
        result.error = L"AI GatewayのAccount ID（32桁の16進数）またはGateway IDが正しく設定されていません。";
        return result;
    }
    if (useGateway && providerSettings->cloudflareApiToken.empty())
    {
        result.error = L"Cloudflare AI Gateway認証用APIトークンが設定されていません。";
        return result;
    }
    HINTERNET session = WinHttpOpen(L"Becky-AI-Assistant/0.88.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { result.error = L"Groq APIへの通信セッションを開始できません。"; return result; }
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 30000);
    const std::wstring gatewayPath = useGateway
        ? L"/v1/" + providerSettings->cloudflareAccountId +
            L"/" + providerSettings->cloudflareGatewayId + L"/groq/chat/completions"
        : L"";
    HINTERNET connection = WinHttpConnect(session,
        useGateway ? L"gateway.ai.cloudflare.com" : L"api.groq.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    const std::wstring requestPath = useGateway ? gatewayPath : L"/openai/v1/chat/completions";
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"POST",
        requestPath.c_str(), nullptr, WINHTTP_NO_REFERER,
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
    if (useGateway)
    {
        if (!providerSettings->cloudflareApiToken.empty())
            headers += L"cf-aig-authorization: Bearer " + providerSettings->cloudflareApiToken + L"\r\n";
        headers += std::wstring(L"cf-aig-collect-log: ") +
            (providerSettings->gatewayRequestLogging ? L"true\r\n" : L"false\r\n");
        headers += L"cf-aig-collect-log-payload: false\r\n";
        if (!providerSettings->gatewayCacheEnabled)
            headers += L"cf-aig-skip-cache: true\r\n";
    }
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
                else
                {
                    std::wstring finish;
                    if (finishReason && ExtractStringProperty(response, "finish_reason", finish))
                        *finishReason = Utf8(finish);
                    result.success = true;
                }
            }
            else
            {
                result.error = useGateway && status == 401
                    ? L"Groq APIキーまたはAI Gateway認証トークンが無効です。両方の設定を確認してください。"
                    : HttpError(status);
                result.retryable = status == 408 || status == 429 || status >= 500;
                std::wstring providerMessage;
                if (ExtractStringProperty(response, "message", providerMessage) && !providerMessage.empty())
                {
                    if (status == 429 &&
                        (providerMessage.find(L"output tokens per minute") != std::wstring::npos ||
                         providerMessage.find(L"OTPM") != std::wstring::npos))
                    {
                        result.error =
                            L"Groqのトークン処理量制限（TPM／OTPM）に達しました。"
                            L"使用枠が回復するまで待ってから自動再試行します。";
                    }
                    result.error += L"\n詳細: " + providerMessage;
                }
            }
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return result;
}

GeminiGenerateResult GenerateGroqChatCompletion(
    const std::wstring& apiKey,
    const std::wstring& modelId,
    const std::wstring& systemPrompt,
    const std::wstring& userPrompt,
    unsigned int maxOutputTokens,
    const std::atomic<int>* cancelState,
    const GroqProgressCallback& progress,
    const AIProviderSettings* providerSettings)
{
    constexpr int kMaxContinuationAttempts = 2;
    GeminiGenerateResult result{ false, 0, {}, L"" };
    std::wstring accumulated;
    for (int attempt = 0; attempt <= kMaxContinuationAttempts; ++attempt)
    {
        if (cancelState && cancelState->load(std::memory_order_acquire) != 0)
        {
            result.cancelled = true;
            result.success = false;
            result.text.clear();
            return result;
        }

        std::string finishReason;
        constexpr int kMaxOtpRetryAttempts = 2;
        int otpRetryAttempt = 0;
        for (;;)
        {
            if (progress)
            {
                if (otpRetryAttempt > 0)
                    progress(L"Groq API再試行中（TPM／OTPM " + std::to_wstring(otpRetryAttempt) + L"/2）");
                else if (attempt > 0)
                    progress(L"回答の続き取得中…");
                else
                    progress(L"Groq API応答待ち");
            }
            result = GenerateGroqChatCompletionOnce(apiKey, modelId, systemPrompt,
                userPrompt, accumulated, maxOutputTokens, &finishReason, cancelState, providerSettings);
            if (result.success || !IsRateLimitError(result.error) ||
                otpRetryAttempt >= kMaxOtpRetryAttempts)
                break;

            const DWORD delayMs = GetRateLimitRetryDelayMs(result.error);
            if (!WaitForRateLimitRetry(delayMs, cancelState, progress, otpRetryAttempt + 1))
            {
                result.success = false;
                result.cancelled = true;
                result.text.clear();
                result.error.clear();
                return result;
            }
            ++otpRetryAttempt;
        }
        if (!result.success)
        {
            // Never expose a partial response as a successful answer.
            result.text.clear();
            if (IsRateLimitError(result.error))
            {
                result.error =
                    L"Groqのトークン処理量制限（TPM／OTPM）に達しました。"
                    L"指定時間待って2回再試行しましたが、制限が解除されませんでした。"
                    L"しばらく時間をおいてから再度お試しください。\n" + result.error;
            }
            return result;
        }

        if (finishReason == "length" && result.text.empty())
        {
            result.success = false;
            result.text.clear();
            result.error = L"Groqの回答が空のまま出力上限に達しました。質問を分けて再度お試しください。";
            return result;
        }

        AppendGeminiContinuationText(accumulated, result.text);
        if (finishReason == "length")
        {
            if (attempt == kMaxContinuationAttempts)
            {
                result.success = false;
                result.text.clear();
                result.error = L"Groqの回答が長文のため、続きを2回生成しましたが完了しませんでした。"
                    L"不完全な回答は表示していません。質問を分けて再度お試しください。";
                return result;
            }
            continue;
        }

        result.text = accumulated;
        result.success = true;
        result.error.clear();
        return result;
    }

    result.success = false;
    result.text.clear();
    result.error = L"Groqの回答を完了できませんでした。質問を分けて再度お試しください。";
    return result;
}
