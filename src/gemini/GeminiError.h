#pragma once

#include <windows.h>
#include <string>

inline std::wstring FormatGeminiHttpError(unsigned long code)
{
    const std::wstring suffix = L"（HTTP " + std::to_wstring(code) + L"）。";
    switch (code)
    {
    case 400:
        return L"リクエストの内容がGemini APIで受け付けられませんでした。モデル設定や入力内容を確認してください。" + suffix;
    case 401:
        return L"APIキーが認証されませんでした。Google AI StudioでAPIキーを確認し、設定し直してください。" + suffix;
    case 403:
        return L"API利用権限がありません。Google AI StudioのプロジェクトとAPI利用権限を確認してください。" + suffix;
    case 404:
        return L"指定したモデルまたはAPIエンドポイントが見つかりません。モデル選択を確認してください。" + suffix;
    case 408:
        return L"Gemini APIへの要求がタイムアウトしました。時間をおいて再試行してください。" + suffix;
    case 413:
        return L"送信データが大きすぎます。メール本文や会話履歴を短くしてください。" + suffix;
    case 429:
        return L"Gemini APIの利用制限または割り当て上限に達しました。利用状況を確認し、時間をおいて再試行してください。" + suffix;
    case 500:
    case 502:
    case 503:
    case 504:
        return L"Gemini API側で一時的な障害が発生しています。モデルを変更するか、時間をおいて再試行してください。" + suffix;
    default:
        if (code >= 500)
            return L"Gemini API側でサーバーエラーが発生しています。時間をおいて再試行してください。" + suffix;
        return L"Gemini APIからエラー応答を受信しました。設定と入力内容を確認してください。" + suffix;
    }
}

inline std::wstring FormatWinHttpError(DWORD error)
{
    switch (error)
    {
    case ERROR_WINHTTP_TIMEOUT:
        return L"Gemini APIへの通信がタイムアウトしました。ネットワーク状態を確認して再試行してください。";
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        return L"Gemini APIの接続先を名前解決できません。DNS設定やインターネット接続を確認してください。";
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_CONNECTION_ERROR:
        return L"Gemini APIへ接続できません。プロキシ、ファイアウォール、インターネット接続を確認してください。";
    case ERROR_WINHTTP_SECURE_FAILURE:
        return L"Gemini APIとのHTTPS接続に失敗しました。PCの日付・時刻、TLS設定、SSL検査やプロキシを確認してください。";
    case ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN:
    case ERROR_WINHTTP_INVALID_URL:
        return L"Gemini APIへの接続設定が正しくありません。";
    default:
        return L"Gemini APIとの通信に失敗しました（Windowsエラー " +
            std::to_wstring(error) +
            L"）。ネットワーク、プロキシ、ファイアウォールを確認してください。";
    }
}
