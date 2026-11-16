#include "ComposeDialog.h"
#include "resource.h"
#include "ProgressIndicator.h"
#include "PiiMaskPreviewDialog.h"
#include "gemini/PiiMasker.h"
#include "settings/ApiKeyStore.h"
#include "gemini/GeminiCompose.h"
#include "gemini/GeminiAsync.h"
#include "gemini/GeminiModels.h"
#include "gemini/GeminiFallback.h"
#include "BeckyAPI.h"
#include <cstring>
#include <commctrl.h>
#include <string>

namespace
{
struct Context { HWND compose; HINSTANCE instance; CBeckyAPI* api; std::vector<GeminiModelOption> models; bool busy = false; bool closeAfterCancel = false; GeminiAsync::WorkItem* work = nullptr; };

std::wstring GetText(HWND dlg, int id)
{
    HWND control = GetDlgItem(dlg, id);
    int len = GetWindowTextLengthW(control);
    std::wstring value((size_t)len + 1, L'\0');
    GetWindowTextW(control, value.data(), len + 1);
    value.resize((size_t)len);
    return value;
}
std::wstring NormalizeWindowsNewlines(const std::wstring& value)
{
    std::wstring normalized;
    normalized.reserve(value.size() + 16);
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == L'\r')
        {
            normalized += L"\r\n";
            if (i + 1 < value.size() && value[i + 1] == L'\n') ++i;
        }
        else if (value[i] == L'\n')
        {
            normalized += L"\r\n";
        }
        else
        {
            normalized += value[i];
        }
    }
    return normalized;
}
std::string ToAnsi(const std::wstring& value)
{
    if (value.empty()) return {};
    int n = WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, out.data(), n, nullptr, nullptr);
    out.resize((size_t)n - 1);
    return out;
}
bool ParseDraft(const std::wstring& raw, std::wstring& subject, std::wstring& body)
{
    const std::wstring subjectTag = L"【件名】";
    const std::wstring bodyTag = L"【本文】";
    size_t s = raw.find(subjectTag), b = raw.find(bodyTag);
    if (s == std::wstring::npos || b == std::wstring::npos || b <= s) return false;
    subject = raw.substr(s + subjectTag.size(), b - (s + subjectTag.size()));
    body = raw.substr(b + bodyTag.size());
    auto trim = [](std::wstring& v) {
        while (!v.empty() && (v.front() == L'\r' || v.front() == L'\n' || v.front() == L' ')) v.erase(v.begin());
        while (!v.empty() && (v.back() == L'\r' || v.back() == L'\n' || v.back() == L' ')) v.pop_back();
    };
    trim(subject); trim(body);
    return !subject.empty() && !body.empty();
}
INT_PTR CALLBACK Proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        auto* c = reinterpret_cast<Context*>(lp);
        SetWindowLongPtrW(dlg, DWLP_USER, (LONG_PTR)c);
        ShowWindow(GetDlgItem(dlg, IDC_NEWMAIL_CANCEL), SW_HIDE);
        SendDlgItemMessageW(dlg, IDC_NEWMAIL_PURPOSE, EM_SETLIMITTEXT, 10000, 0);
        SendDlgItemMessageW(dlg, IDC_NEWMAIL_REQUIREMENTS, EM_SETLIMITTEXT, 30000, 0);
        std::wstring modelError;
        if (!LoadGeminiModelCatalog(c->models, modelError) || c->models.empty())
            c->models = DefaultGeminiModels();
        int defaultIndex = 0;
        for (size_t i = 0; i < c->models.size(); ++i)
        {
            SendDlgItemMessageW(dlg, IDC_NEWMAIL_MODEL, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(c->models[i].label.c_str()));
            if (c->models[i].id == L"gemini-3.5-flash-lite") defaultIndex = static_cast<int>(i);
        }
        SendDlgItemMessageW(dlg, IDC_NEWMAIL_MODEL, CB_SETCURSEL, defaultIndex, 0);
        SendMessageW(GetDlgItem(dlg, IDC_NEWMAIL_TONE), TBM_SETRANGE, TRUE, MAKELONG(1, 10));
        SendMessageW(GetDlgItem(dlg, IDC_NEWMAIL_TONE), TBM_SETPOS, TRUE, 5);
        SetDlgItemTextW(dlg, IDC_NEWMAIL_STATUS, L"メールの目的を入力して「文案を作成」を押してください。");
        return TRUE;
    }
    if (msg == GeminiAsync::WM_GENERATION_COMPLETE)
    {
        auto* c = reinterpret_cast<Context*>(GetWindowLongPtrW(dlg, DWLP_USER));
        auto* work = reinterpret_cast<GeminiAsync::WorkItem*>(lp);
        if (!work) return TRUE;
        if (c) {
            c->busy = false;
            c->work = nullptr;
            ShowWindow(GetDlgItem(dlg, IDC_NEWMAIL_CANCEL), SW_HIDE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_GENERATE), TRUE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_APPLY), TRUE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_PURPOSE), TRUE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_REQUIREMENTS), TRUE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_MODEL), TRUE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_TONE), TRUE);
            const GeminiGenerateResult& result = work->result;
            if (result.cancelled) {
                SetDlgItemTextW(dlg, IDC_NEWMAIL_STATUS, L"生成処理をキャンセルしました。");
                const bool closeNow = c->closeAfterCancel;
                c->closeAfterCancel = false;
                GeminiAsync::Destroy(work);
                if (closeNow) EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            c->closeAfterCancel = false;
            if (!result.success) {
                SetDlgItemTextW(dlg, IDC_NEWMAIL_STATUS, L"文案の作成に失敗しました。");
                MessageBoxW(dlg, result.error.c_str(), L"Gemini APIエラー", MB_OK | MB_ICONWARNING);
            } else {
                std::wstring subject, body;
                if (!ParseDraft(result.text, subject, body)) {
                    SetDlgItemTextW(dlg, IDC_NEWMAIL_STATUS, L"応答形式を解析できませんでした。");
                    SetDlgItemTextW(dlg, IDC_NEWMAIL_BODY, result.text.c_str());
                } else {
                    SetDlgItemTextW(dlg, IDC_NEWMAIL_SUBJECT, subject.c_str());
                    const std::wstring displayBody = NormalizeWindowsNewlines(body);
                    SetDlgItemTextW(dlg, IDC_NEWMAIL_BODY, displayBody.c_str());
                    SetDlgItemTextW(dlg, IDC_NEWMAIL_STATUS, result.notice.empty()
                        ? L"生成しました。件名・本文を編集してから反映してください。"
                        : result.notice.c_str());
                }
            }
        }
        GeminiAsync::Destroy(work);
        return TRUE;
    }
    if (msg == WM_COMMAND)
    {
        switch (LOWORD(wp))
        {
        case IDC_NEWMAIL_GENERATE:
        {
            auto* c = reinterpret_cast<Context*>(GetWindowLongPtrW(dlg, DWLP_USER));
            if (!c || c->busy) return TRUE;
            const std::wstring purpose = GetText(dlg, IDC_NEWMAIL_PURPOSE);
            const std::wstring requirements = GetText(dlg, IDC_NEWMAIL_REQUIREMENTS);
            if (purpose.empty()) { MessageBoxW(dlg, L"メールの目的を入力してください。", L"AIメール作成", MB_OK | MB_ICONWARNING); return TRUE; }
            LRESULT selected = SendDlgItemMessageW(dlg, IDC_NEWMAIL_MODEL, CB_GETCURSEL, 0, 0);
            if (selected < 0 || selected >= (LRESULT)c->models.size()) return TRUE;
            const int tone = (int)SendDlgItemMessageW(dlg, IDC_NEWMAIL_TONE, TBM_GETPOS, 0, 0);
            const wchar_t* toneText[] = {
                L"格式の高いビジネス文書として、改まった敬語を使ってください。",
                L"かなりフォーマルで丁重な敬語を使ってください。",
                L"フォーマルで礼儀正しいビジネスメールにしてください。",
                L"標準より少し改まった丁寧なビジネスメールにしてください。",
                L"標準的で丁寧なビジネス敬語を使い、自然で簡潔にしてください。",
                L"丁寧さを保ちながら、少し柔らかく自然な表現にしてください。",
                L"丁寧さを保ちつつ、親しみやすく柔らかい表現にしてください。",
                L"柔らかく親しみやすい表現を使い、失礼のない敬語を維持してください。",
                L"必要な要点を優先し、回りくどい表現を避けて短くしてください。",
                L"必要な内容だけを最短で伝え、失礼のない敬語を維持してください。"
            };
            PiiMasker masker;
            if (!masker.IsReady())
            {
                MessageBoxW(dlg, masker.Error().c_str(), L"マスキング設定エラー", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            const std::wstring previewRaw =
                L"【メールの目的】\r\n" + purpose +
                L"\r\n\r\n【補足・含めたい内容・条件】\r\n" + requirements;
            const std::wstring maskedPreview = masker.Mask(previewRaw);
            if (masker.IsEnabled() && !ShowPiiMaskPreviewDialog(dlg, c->instance,
                L"新規メール作成：Gemini送信内容の確認", maskedPreview, masker)) return TRUE;
            std::wstring key, error;
            if (!LoadGeminiApiKey(key, error) || key.empty()) {
                MessageBoxW(dlg, error.empty() ? L"先に設定画面でGemini APIキーを保存してください。" : error.c_str(),
                    L"Becky AI Assistant", MB_OK | MB_ICONWARNING); return TRUE;
            }
            const auto& model = c->models[static_cast<size_t>(selected)];
            const std::wstring modelId = model.id;
            const std::wstring thinking = model.thinking;
            const auto models = c->models;
            GeminiReliabilityOptions reliability;
            std::wstring reliabilityError;
            if (!LoadGeminiReliabilityOptions(reliability, reliabilityError)) {
                MessageBoxW(dlg, reliabilityError.c_str(), L"設定エラー", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            const bool autoFallback = reliability.automaticModelFallback;
            const std::wstring toneInstruction = toneText[(tone < 1 || tone > 10) ? 4 : tone - 1];
            c->busy = true;
            ShowWindow(GetDlgItem(dlg, IDC_NEWMAIL_CANCEL), SW_SHOW);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_CANCEL), TRUE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_GENERATE), FALSE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_APPLY), FALSE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_PURPOSE), FALSE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_REQUIREMENTS), FALSE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_MODEL), FALSE);
            EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_TONE), FALSE);
            ShowApiProcessingStatus(dlg, IDC_NEWMAIL_STATUS, L"Geminiで文案を作成しています...");
            const bool started = GeminiAsync::Start(dlg, std::move(key),
                [modelId, thinking, models, autoFallback, toneInstruction, purpose, requirements, masker]
                (const std::wstring& workerKey, const std::atomic<int>& cancelState) mutable {
                    return GenerateWithModelFallback(modelId, thinking, models, autoFallback,
                        [&](const std::wstring& candidateId, const std::wstring& candidateThinking) {
                            return GenerateNewMailDraft(workerKey, candidateId, candidateThinking,
                                toneInstruction, purpose, requirements, &masker);
                        }, &cancelState);
                }, 0, &c->work);
            if (!started) {
                c->busy = false;
                c->work = nullptr;
                ShowWindow(GetDlgItem(dlg, IDC_NEWMAIL_CANCEL), SW_HIDE);
                EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_GENERATE), TRUE);
                EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_APPLY), TRUE);
                EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_PURPOSE), TRUE);
                EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_REQUIREMENTS), TRUE);
                EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_MODEL), TRUE);
                EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_TONE), TRUE);
                SetDlgItemTextW(dlg, IDC_NEWMAIL_STATUS, L"バックグラウンド処理を開始できませんでした。");
                MessageBoxW(dlg, L"非同期処理を開始できませんでした。もう一度お試しください。",
                    L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
            }
            return TRUE;
        }
        case IDC_NEWMAIL_CANCEL:
        {
            auto* c = reinterpret_cast<Context*>(GetWindowLongPtrW(dlg, DWLP_USER));
            if (c && c->busy && c->work) {
                EnableWindow(GetDlgItem(dlg, IDC_NEWMAIL_CANCEL), FALSE);
                SetDlgItemTextW(dlg, IDC_NEWMAIL_STATUS, L"キャンセルしています...");
                GeminiAsync::RequestCancel(c->work);
            }
            return TRUE;
        }
        case IDC_NEWMAIL_APPLY:
        {
            auto* c = reinterpret_cast<Context*>(GetWindowLongPtrW(dlg, DWLP_USER));
            if (!c) return TRUE;
            const std::wstring subject = GetText(dlg, IDC_NEWMAIL_SUBJECT);
            const std::wstring body = GetText(dlg, IDC_NEWMAIL_BODY);
            if (subject.empty() || body.empty()) {
                MessageBoxW(dlg, L"件名と本文の両方を入力してください。", L"AIメール作成", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            char oldSubject[2048]{}, mime[256]{};
            c->api->CompGetSpecifiedHeader(c->compose, "Subject", oldSubject, (int)sizeof(oldSubject));
            oldSubject[sizeof(oldSubject) - 1] = '\0';
            LPSTR oldBodyPtr = c->api->CompGetText(c->compose, mime, (int)sizeof(mime));
            const bool hasOldBody = oldBodyPtr != nullptr && oldBodyPtr[0] != '\0';
            const bool hasOldSubject = oldSubject[0] != '\0';
            if (hasOldSubject || hasOldBody) {
                const int confirm = MessageBoxW(dlg,
                    L"作成画面には既に件名または本文が入力されています。\n\n"
                    L"AI文案を反映すると、件名と本文を置き換えます。続行しますか？",
                    L"既存内容の置き換え確認", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (confirm != IDYES) {
                    if (oldBodyPtr) c->api->Free(oldBodyPtr);
                    return TRUE;
                }
            }
            if (oldBodyPtr) c->api->Free(oldBodyPtr);
            const std::string subjectAnsi = ToAnsi(subject);
            const std::string bodyAnsi = ToAnsi(body);
            if (subjectAnsi.empty() || bodyAnsi.empty()) {
                MessageBoxW(dlg, L"件名または本文をBecky!の文字コードに変換できませんでした。",
                    L"AIメール作成", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            c->api->CompSetSpecifiedHeader(c->compose, "Subject", subjectAnsi.c_str());
            c->api->CompSetText(c->compose, 0, bodyAnsi.c_str());
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
        case IDOK:
            if (auto* c = reinterpret_cast<Context*>(GetWindowLongPtrW(dlg, DWLP_USER)); c && c->busy) { c->closeAfterCancel = true; if (c->work) GeminiAsync::RequestCancel(c->work); SetDlgItemTextW(dlg, IDC_NEWMAIL_STATUS, L"キャンセルして終了しています..."); return TRUE; }
            EndDialog(dlg, LOWORD(wp)); return TRUE;
        }
    }
    if (msg == WM_CLOSE) {
        auto* c = reinterpret_cast<Context*>(GetWindowLongPtrW(dlg, DWLP_USER));
        if (c && c->busy) { c->closeAfterCancel = true; if (c->work) GeminiAsync::RequestCancel(c->work); SetDlgItemTextW(dlg, IDC_NEWMAIL_STATUS, L"キャンセルして終了しています..."); return TRUE; }
        EndDialog(dlg, IDCANCEL); return TRUE;
    }
    return FALSE;
}
}

INT_PTR ShowNewMailComposeDialog(HWND composeWindow, HINSTANCE instance, CBeckyAPI& beckyApi)
{
    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_BAR_CLASSES };
    InitCommonControlsEx(&controls);
    Context context{ composeWindow, instance, &beckyApi };
    return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_NEWMAIL_COMPOSE),
        composeWindow, Proc, (LPARAM)&context);
}
