#include "LauncherDialog.h"
#include "SettingsDialog.h"
#include "ResultDialog.h"
#include "ConsultDialog.h"
#include "resource.h"
#include "settings/ApiKeyStore.h"
#include "gemini/GeminiGenerate.h"
#include "gemini/GeminiReply.h"
#include <utility>
#include <commctrl.h>

bool OpenReplyComposer(const std::wstring& mailId, const std::wstring& replyText);

namespace
{
    struct ModelOption
    {
        const wchar_t* label;
        const wchar_t* modelId;
        const wchar_t* thinking;
    };

    const ModelOption kModels[] = {
        { L"Gemini 3.8 Flash (Thinking High)", L"gemini-3.8-flash", L"HIGH" },
        { L"Gemini 3.8 Flash (Thinking Medium)", L"gemini-3.8-flash", L"MEDIUM" },
        { L"Gemini 3.8 Flash (Thinking Low)", L"gemini-3.8-flash", L"LOW" },
        { L"Gemini 3.6 Flash (Thinking High)", L"gemini-3.6-flash", L"HIGH" },
        { L"Gemini 3.6 Flash (Thinking Medium)", L"gemini-3.6-flash", L"MEDIUM" },
        { L"Gemini 3.6 Flash (Thinking Low)", L"gemini-3.6-flash", L"LOW" },
        { L"Gemini 3.6 Flash (Thinking Minimal)", L"gemini-3.6-flash", L"MINIMAL" },
        { L"Gemini 3.5 Flash-Lite (Thinking High)", L"gemini-3.5-flash-lite", L"HIGH" },
        { L"Gemini 3.5 Flash-Lite (Thinking Medium)", L"gemini-3.5-flash-lite", L"MEDIUM" },
        { L"Gemini 3.5 Flash-Lite (Thinking Low)", L"gemini-3.5-flash-lite", L"LOW" },
        { L"Gemini 3.5 Flash-Lite (Thinking Minimal)", L"gemini-3.5-flash-lite", L"MINIMAL" },
        { L"Gemini 2.5 Pro (Thinking On)", L"gemini-2.5-pro", L"ON" },
        { L"Gemini 2.5 Flash (Thinking On)", L"gemini-2.5-flash", L"ON" },
        { L"Gemini 2.5 Flash (Thinking Off)", L"gemini-2.5-flash", L"OFF" },
        { L"Gemini 2.5 Flash-Lite (Thinking On)", L"gemini-2.5-flash-lite", L"ON" },
        { L"Gemini 2.5 Flash-Lite (Thinking Off)", L"gemini-2.5-flash-lite", L"OFF" },
        { L"Gemini 3.1 Pro Preview (Thinking High)", L"gemini-3.1-pro-preview", L"HIGH" },
        { L"Gemini 3.1 Pro Preview (Thinking Medium)", L"gemini-3.1-pro-preview", L"MEDIUM" },
        { L"Gemini 3.1 Pro Preview (Thinking Low)", L"gemini-3.1-pro-preview", L"LOW" },
        { L"Gemma 4 31B (Thinking High)", L"gemma-4-31b-it", L"" },
        { L"Gemma 4 31B (Thinking Minimal)", L"gemma-4-31b-it", L"" }
    };

    const wchar_t* ReplyToneLabel(int level)
    {
        static const wchar_t* labels[] = {
            L"強ビジネス", L"かなり堅め", L"フォーマル", L"やや堅め",
            L"標準", L"少し柔らか", L"やや柔らか", L"柔らかめ",
            L"簡潔", L"最短"
        };
        if (level < 1) level = 1;
        if (level > 10) level = 10;
        return labels[level - 1];
    }

    const wchar_t* GetReplyToneInstruction(int level)
    {
        static const wchar_t* instructions[] = {
            L"格式の高いビジネス文書として作成してください。敬語・謙譲語を適切に使い、改まった表現を用いてください。",
            L"かなりフォーマルなビジネスメールにしてください。丁重な敬語を使い、くだけた表現は避けてください。",
            L"フォーマルで礼儀正しいビジネスメールにしてください。丁寧な敬語と落ち着いた表現を使ってください。",
            L"標準より少し改まった丁寧なビジネスメールにしてください。礼儀正しさを重視してください。",
            L"標準的で丁寧なビジネス敬語を使い、自然で簡潔な文章にしてください。",
            L"丁寧さを保ちながら、少し柔らかく自然な表現にしてください。",
            L"丁寧さを保ちつつ、親しみやすく柔らかい表現にしてください。過度にくだけないでください。",
            L"柔らかく親しみやすい表現を使い、要点を簡潔にまとめてください。失礼のない敬語は維持してください。",
            L"必要な要点を優先し、回りくどい前置きや重複を避けて、短く簡潔な文章にしてください。",
            L"最も簡潔な返信にしてください。必要な内容だけを短く伝え、失礼のない敬語は維持してください。"
        };
        if (level < 1) level = 1;
        if (level > 10) level = 10;
        return instructions[level - 1];
    }

    void UpdateReplyToneDisplay(HWND dialog)
    {
        const int level = static_cast<int>(SendDlgItemMessageW(
            dialog, IDC_REPLY_TONE, TBM_GETPOS, 0, 0));
        wchar_t label[128] = {};
        wsprintfW(label, L"%d/10 %s", level, ReplyToneLabel(level));
        SetDlgItemTextW(dialog, IDC_REPLY_TONE_LABEL, label);
    }

    void InitializeReplyToneTrackbar(HWND dialog)
    {
        HWND slider = GetDlgItem(dialog, IDC_REPLY_TONE);
        SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELONG(1, 10));
        SendMessageW(slider, TBM_SETTICFREQ, 1, 0);
        SendMessageW(slider, TBM_SETPOS, TRUE, 5);
        UpdateReplyToneDisplay(dialog);
    }

    struct DialogContext
    {
        MailContext* mail;
        HINSTANCE instance;
    };

    void InitializeModelCombo(HWND dialog)
    {
        HWND combo = GetDlgItem(dialog, IDC_GEMINI_MODEL);
        for (const auto& model : kModels)
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(model.label));
        // Default to the model that the user has confirmed works in the other system.
        SendMessageW(combo, CB_SETCURSEL, 8, 0);
    }

    INT_PTR CALLBACK LauncherDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
        {
            auto* context = reinterpret_cast<DialogContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
            InitializeModelCombo(dialog);
            InitializeReplyToneTrackbar(dialog);
            const auto* mail = context ? context->mail : nullptr;
            if (mail != nullptr)
            {
                SetDlgItemTextW(dialog, IDC_MAIL_SUBJECT,
                    mail->subject.empty() ? L"(件名を取得できません)" : mail->subject.c_str());
                SetDlgItemTextW(dialog, IDC_MAIL_FROM,
                    mail->from.empty() ? L"(差出人を取得できません)" : mail->from.c_str());
                SetDlgItemTextW(dialog, IDC_MAIL_BODY,
                    mail->body.empty() ? L"(本文を表示できません)" : mail->body.c_str());
                SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS,
                    mail->bodyStatus.empty() ? L"" : mail->bodyStatus.c_str());
            }
            return TRUE;
        }

        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(lParam) == GetDlgItem(dialog, IDC_REPLY_TONE))
                UpdateReplyToneDisplay(dialog);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_AI_SUMMARY:
            {
                auto* context = reinterpret_cast<DialogContext*>(
                    GetWindowLongPtrW(dialog, DWLP_USER));
                if (context == nullptr || context->mail == nullptr) return TRUE;
                const MailContext& mail = *context->mail;
                const int consent = MessageBoxW(dialog,
                    L"次の情報をGoogle Gemini APIへ送信して要約します。\n\n"
                    L"・メールの件名\n・差出人\n・メール本文\n\n"
                    L"メール本文には機密情報が含まれる可能性があります。"
                    L"送信先と内容を確認し、許可する場合のみ「はい」を選択してください。"
                    L"キャンセルすると外部送信しません。",
                    L"メール本文の外部送信確認",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (consent != IDYES) return TRUE;

                const LRESULT selected = SendDlgItemMessageW(dialog, IDC_GEMINI_MODEL, CB_GETCURSEL, 0, 0);
                if (selected < 0 || selected >= static_cast<LRESULT>(_countof(kModels)))
                {
                    MessageBoxW(dialog, L"Geminiモデルを選択してください。",
                        L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }

                std::wstring apiKey, error;
                if (!LoadGeminiApiKey(apiKey, error) || apiKey.empty())
                {
                    MessageBoxW(dialog,
                        error.empty() ? L"先に設定画面でGemini APIキーを保存してください。" : error.c_str(),
                        L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS,
                    L"Geminiで要約中です。しばらくお待ちください...");
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), FALSE);
                const auto& model = kModels[selected];
                GeminiGenerateResult result = GenerateMailSummary(
                    apiKey, model.modelId, model.thinking, mail.subject, mail.from, mail.body);
                if (!apiKey.empty()) SecureZeroMemory(apiKey.data(), apiKey.size() * sizeof(wchar_t));
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), TRUE);
                SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"");
                if (result.success)
                    ShowResultDialog(dialog, context->instance, result.text);
                else
                    MessageBoxW(dialog, result.error.c_str(),
                        L"Gemini APIエラー", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            case IDC_AI_REPLY:
            {
                auto* context = reinterpret_cast<DialogContext*>(
                    GetWindowLongPtrW(dialog, DWLP_USER));
                if (context == nullptr || context->mail == nullptr) return TRUE;
                const MailContext& mail = *context->mail;
                const LRESULT selected = SendDlgItemMessageW(dialog, IDC_GEMINI_MODEL, CB_GETCURSEL, 0, 0);
                if (selected < 0 || selected >= static_cast<LRESULT>(_countof(kModels)))
                {
                    MessageBoxW(dialog, L"Geminiモデルを選択してください。",
                        L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const LRESULT toneSelected = SendDlgItemMessageW(dialog, IDC_REPLY_TONE, TBM_GETPOS, 0, 0);
                if (toneSelected < 1 || toneSelected > 10)
                {
                    MessageBoxW(dialog, L"返信文案のトーンを選択してください。",
                        L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const int consent = MessageBoxW(dialog,
                    L"次の情報をGoogle Gemini APIへ送信して返信文案を作成します。\n\n"
                    L"・メールの件名\n・差出人\n・メール本文\n\n"
                    L"メール本文には機密情報が含まれる可能性があります。"
                    L"送信内容を確認し、許可する場合のみ「はい」を選択してください。"
                    L"キャンセルすると外部送信しません。",
                    L"返信文案作成 - 外部送信確認",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (consent != IDYES) return TRUE;

                std::wstring apiKey, error;
                if (!LoadGeminiApiKey(apiKey, error) || apiKey.empty())
                {
                    MessageBoxW(dialog,
                        error.empty() ? L"先に設定画面でGemini APIキーを保存してください。" : error.c_str(),
                        L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS,
                    L"Geminiで返信文案を作成中です。しばらくお待ちください...");
                EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), FALSE);
                const auto& model = kModels[selected];
                GeminiGenerateResult result = GenerateMailReplyDraft(
                    apiKey, model.modelId, model.thinking, GetReplyToneInstruction(static_cast<int>(toneSelected)), mail.subject, mail.from, mail.body);
                if (!apiKey.empty()) SecureZeroMemory(apiKey.data(), apiKey.size() * sizeof(wchar_t));
                EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), TRUE);
                SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"");
                if (result.success)
                    {
                    std::wstring edited;
                    if (ShowReplyDraftDialog(dialog, context->instance, result.text, edited)) {
                        context->mail->replyDraft = std::move(edited);
                        context->mail->insertReplyRequested = true;
                        EndDialog(dialog, IDOK);
                    }
                }
                else
                    MessageBoxW(dialog, result.error.c_str(),
                        L"Gemini APIエラー", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            case IDC_AI_CONSULT:
            {
                auto* context = reinterpret_cast<DialogContext*>(
                    GetWindowLongPtrW(dialog, DWLP_USER));
                if (context == nullptr || context->mail == nullptr) return TRUE;
                const LRESULT selected = SendDlgItemMessageW(dialog, IDC_GEMINI_MODEL, CB_GETCURSEL, 0, 0);
                if (selected < 0 || selected >= static_cast<LRESULT>(_countof(kModels)))
                {
                    MessageBoxW(dialog, L"Geminiモデルを選択してください。",
                        L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const auto& model = kModels[selected];
                ShowConsultDialog(dialog, context->instance, *context->mail, model.modelId, model.thinking);
                return TRUE;
            }

            case IDC_AI_SETTINGS:
            {
                auto* context = reinterpret_cast<DialogContext*>(
                    GetWindowLongPtrW(dialog, DWLP_USER));
                if (context != nullptr)
                    ShowSettingsDialog(dialog, context->instance);
                return TRUE;
            }

            case IDCANCEL:
            case IDOK:
                EndDialog(dialog, LOWORD(wParam));
                return TRUE;

            default:
                break;
            }
            break;

        case WM_CLOSE:
            EndDialog(dialog, IDCANCEL);
            return TRUE;

        default:
            break;
        }
        return FALSE;
    }
}

INT_PTR ShowLauncherDialog(HWND owner, HINSTANCE instance, MailContext& mail)
{
    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_BAR_CLASSES };
    InitCommonControlsEx(&controls);
    DialogContext context{ &mail, instance };
    return DialogBoxParamW(
        instance,
        MAKEINTRESOURCEW(IDD_AI_LAUNCHER),
        owner,
        LauncherDialogProc,
        reinterpret_cast<LPARAM>(&context));
}
