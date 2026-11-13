#include "GeminiConnection.h"

#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace
{
std::wstring StatusMessage(GeminiConnectionStatus status)
{
    switch (status)
    {
    case GeminiConnectionStatus::Success: return L"Gemini APIへの接続に成功しました。APIキーは有効です。";
    case GeminiConnectionStatus::MissingApiKey: return L"APIキーが設定されていません。先にAPIキーを保存してください。";
    case GeminiConnectionStatus::InvalidApiKey: return L"APIキーが無効です。Google AI Studioでキーを確認してください。";
    case GeminiConnectionStatus::PermissionDenied: return L"APIキーに必要な権限がありません。Google AI Studioの設定を確認してください。";
    case GeminiConnectionStatus::RateLimited: return L"APIの利用制限に達しています。時間をおいて再試行してください。";
    case GeminiConnectionStatus::ServerError: return L"Gemini API側でエラーが発生しました。時間をおいて再試行してください。";
    case GeminiConnectionStatus::NetworkError: return L"通信に失敗しました。インターネット接続やプロキシ・ファイアウォールを確認してください。";
    default: return L"Gemini APIから予期しない応答がありました。";
    }
}

bool IsAsciiKey(const std::wstring& key)
{
    return std::all_of(key.begin(), key.end(), [](wchar_t c) {
        return c >= 0x21 && c <= 0x7e;
    });
}
}

GeminiConnectionResult TestGeminiConnection(const std::wstring& apiKey)
{
    GeminiConnectionResult result{ GeminiConnectionStatus::NetworkError, 0, L"" };
    if (apiKey.empty())
    {
        result.status = GeminiConnectionStatus::MissingApiKey;
        result.message = StatusMessage(result.status);
        return result;
    }
    if (!IsAsciiKey(apiKey))
    {
        result.status = GeminiConnectionStatus::InvalidApiKey;
        result.message = L"APIキーに使用できない文字が含まれています。";
        return result;
    }

    HINTERNET session = WinHttpOpen(L"Becky-AI-Assistant/0.4",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
    {
        result.message = StatusMessage(result.status);
        return result;
    }

    WinHttpSetTimeouts(session, 10000, 10000, 10000, 15000);
    HINTERNET connection = WinHttpConnect(session,
        L"generativelanguage.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection)
    {
        WinHttpCloseHandle(session);
        result.message = StatusMessage(result.status);
        return result;
    }

    HINTERNET request = WinHttpOpenRequest(connection, L"GET",
        L"/v1beta/models?pageSize=100", nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request)
    {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        result.message = StatusMessage(result.status);
        return result;
    }

    std::wstring headers = L"x-goog-api-key: " + apiKey + L"\r\nAccept: application/json\r\n";
    BOOL sent = WinHttpSendRequest(request, headers.c_str(),
        static_cast<DWORD>(headers.size()), WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    BOOL received = sent ? WinHttpReceiveResponse(request, nullptr) : FALSE;
    if (!received)
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        result.message = StatusMessage(result.status);
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    result.httpStatus = statusCode;

    std::string response;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0)
    {
        if (response.size() + available > 1024 * 1024) break;
        const size_t oldSize = response.size();
        response.resize(oldSize + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, response.data() + oldSize, available, &read))
        {
            response.resize(oldSize);
            break;
        }
        response.resize(oldSize + read);
        if (read == 0) break;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (statusCode == 200)
    {
        if (response.find("\"models\"") != std::string::npos)
            result.status = GeminiConnectionStatus::Success;
        else
            result.status = GeminiConnectionStatus::UnexpectedResponse;
    }
    else if (statusCode == 400 || statusCode == 401)
        result.status = GeminiConnectionStatus::InvalidApiKey;
    else if (statusCode == 403)
        result.status = GeminiConnectionStatus::PermissionDenied;
    else if (statusCode == 429)
        result.status = GeminiConnectionStatus::RateLimited;
    else if (statusCode >= 500)
        result.status = GeminiConnectionStatus::ServerError;
    else
        result.status = GeminiConnectionStatus::UnexpectedResponse;

    result.message = StatusMessage(result.status);
    return result;
}
