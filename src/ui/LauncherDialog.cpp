#include "LauncherDialog.h"
#include "ResultDialog.h"
#include "ConsultDialog.h"
#include "resource.h"
#include "ProgressIndicator.h"
#include "PiiMaskPreviewDialog.h"
#include "gemini/PiiMasker.h"
#include "settings/ApiKeyStore.h"
#include "gemini/GeminiGenerate.h"
#include "gemini/GeminiReply.h"
#include "gemini/GeminiModels.h"
#include <utility>
#include <commctrl.h>

bool OpenReplyComposer(const std::wstring& mailId, const std::wstring& replyText);

namespace
{
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
        std::vector<GeminiModelOption> models;
    };

    void InitializeModelCombo(HWND dialog, DialogContext* context)
    {
        if (!context) return;
        std::wstring error;
        if (!LoadGeminiModelCatalog(context->models, error) || context->models.empty())
            context->models = DefaultGeminiModels();
        HWND combo = GetDlgItem(dialog, IDC_GEMINI_MODEL);
        int defaultIndex = 0;
        for (size_t i = 0; i < context->models.size(); ++i)
        {
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(context->models[i].label.c_str()));
            if (context->models[i].id == L"gemini-3.8-flash") defaultIndex = static_cast<int>(i);
        }
        SendMessageW(combo, CB_SETCURSEL, defaultIndex, 0);
    }

    INT_PTR CALLBACK LauncherDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
        {
            auto* context = reinterpret_cast<DialogContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
            InitializeModelCombo(dialog, context);
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
                PiiMasker masker;
                if (!masker.IsReady())
                {
                    MessageBoxW(dialog, masker.Error().c_str(), L"マスキング設定エラー", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const std::wstring previewRaw =
                    L"【件名】\r\n" + mail.subject + L"\r\n\r\n【差出人】\r\n" +
                    mail.from + L"\r\n\r\n【本文】\r\n" + mail.body;
                const std::wstring maskedPreview = masker.Mask(previewRaw);
                if (!ShowPiiMaskPreviewDialog(dialog, context->instance,
                    L"要約：Gemini送信内容の確認", maskedPreview, masker)) return TRUE;

                const LRESULT selected = SendDlgItemMessageW(dialog, IDC_GEMINI_MODEL, CB_GETCURSEL, 0, 0);
                if (selected < 0 || selected >= static_cast<LRESULT>(context->models.size()))
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
                ShowApiProcessingStatus(dialog, IDC_MAIL_BODY_STATUS,
                    L"Geminiで要約中です。しばらくお待ちください...");
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), FALSE);
                const auto& model = context->models[static_cast<size_t>(selected)];
                GeminiGenerateResult result = GenerateMailSummary(
                    apiKey, model.id, model.thinking, mail.subject, mail.from, mail.body, &masker);
                if (!apiKey.empty()) SecureZeroMemory(apiKey.data(), apiKey.size() * sizeof(wchar_t));
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), TRUE);
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
                if (selected < 0 || selected >= static_cast<LRESULT>(context->models.size()))
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
                PiiMasker masker;
                if (!masker.IsReady())
                {
                    MessageBoxW(dialog, masker.Error().c_str(), L"マスキング設定エラー", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const std::wstring previewRaw =
                    L"【件名】\r\n" + mail.subject + L"\r\n\r\n【差出人】\r\n" +
                    mail.from + L"\r\n\r\n【本文】\r\n" + mail.body;
                const std::wstring maskedPreview = masker.Mask(previewRaw);
                if (!ShowPiiMaskPreviewDialog(dialog, context->instance,
                    L"返信文案：Gemini送信内容の確認", maskedPreview, masker)) return TRUE;

                std::wstring apiKey, error;
                if (!LoadGeminiApiKey(apiKey, error) || apiKey.empty())
                {
                    MessageBoxW(dialog,
                        error.empty() ? L"先に設定画面でGemini APIキーを保存してください。" : error.c_str(),
                        L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                ShowApiProcessingStatus(dialog, IDC_MAIL_BODY_STATUS,
                    L"Geminiで返信文案を作成中です。しばらくお待ちください...");
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), FALSE);
                const auto& model = context->models[static_cast<size_t>(selected)];
                GeminiGenerateResult result = GenerateMailReplyDraft(
                    apiKey, model.id, model.thinking, GetReplyToneInstruction(static_cast<int>(toneSelected)), mail.subject, mail.from, mail.body, L"", &masker);
                if (!apiKey.empty()) SecureZeroMemory(apiKey.data(), apiKey.size() * sizeof(wchar_t));
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), TRUE);
                SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"");
                if (result.success)
                    {
                    std::wstring edited;
                    bool replyAll = false;
                    if (ShowReplyDraftDialog(dialog, context->instance, result.text, edited, replyAll)) {
                        context->mail->replyDraft = std::move(edited);
                        context->mail->insertReplyRequested = true;
                        context->mail->replyAllRequested = replyAll;
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
                if (selected < 0 || selected >= static_cast<LRESULT>(context->models.size()))
                {
                    MessageBoxW(dialog, L"Geminiモデルを選択してください。",
                        L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const auto& model = context->models[static_cast<size_t>(selected)];
                const int toneLevel = static_cast<int>(SendDlgItemMessageW(dialog, IDC_REPLY_TONE, TBM_GETPOS, 0, 0));
                ShowConsultDialog(dialog, context->instance, *context->mail,
                    model.id, model.thinking, GetReplyToneInstruction(toneLevel));
                if (context->mail->insertReplyRequested)
                    EndDialog(dialog, IDOK);
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
    DialogContext context{ &mail, instance, {} };
    return DialogBoxParamW(
        instance,
        MAKEINTRESOURCEW(IDD_AI_LAUNCHER),
        owner,
        LauncherDialogProc,
        reinterpret_cast<LPARAM>(&context));
}
