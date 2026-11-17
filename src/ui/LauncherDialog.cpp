#include "LauncherDialog.h"
#include "ResultDialog.h"
#include "ConsultDialog.h"
#include "resource.h"
#include "ProgressIndicator.h"
#include "PiiMaskPreviewDialog.h"
#include "gemini/PiiMasker.h"
#include "settings/ApiKeyStore.h"
#include "gemini/GeminiGenerate.h"
#include "gemini/GeminiAsync.h"
#include "gemini/GeminiReply.h"
#include "gemini/GeminiModels.h"
#include "gemini/GeminiFallback.h"
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
        bool busy = false;
        bool closeAfterCancel = false;
        GeminiAsync::WorkItem* work = nullptr;
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
            if (context->models[i].id == L"gemini-3.5-flash-lite") defaultIndex = static_cast<int>(i);
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
            PiiMaskOptions maskOptions;
            std::wstring maskError;
            if (LoadPiiMaskOptions(maskOptions, maskError))
                CheckDlgButton(dialog, IDC_MASK_ENABLED, maskOptions.enabled ? BST_CHECKED : BST_UNCHECKED);
            else
                SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, maskError.c_str());
            ShowWindow(GetDlgItem(dialog, IDC_AI_CANCEL_GENERATION), SW_HIDE);
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

        case GeminiAsync::WM_GENERATION_COMPLETE:
        {
            auto* context = reinterpret_cast<DialogContext*>(GetWindowLongPtrW(dialog, DWLP_USER));
            auto* work = reinterpret_cast<GeminiAsync::WorkItem*>(lParam);
            if (!work) return TRUE;
            if (context) {
                context->busy = false;
                context->work = nullptr;
                ShowWindow(GetDlgItem(dialog, IDC_AI_CANCEL_GENERATION), SW_HIDE);
                if (work->result.cancelled) {
                    const bool closeNow = context->closeAfterCancel;
                    context->closeAfterCancel = false;
                    EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_GEMINI_MODEL), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_MASK_ENABLED), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_REPLY_TONE), TRUE);
                    SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"生成処理をキャンセルしました。");
                    GeminiAsync::Destroy(work);
                    if (closeNow) EndDialog(dialog, IDCANCEL);
                    return TRUE;
                }
                context->closeAfterCancel = false;
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_GEMINI_MODEL), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_MASK_ENABLED), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_REPLY_TONE), TRUE);
                SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"");
                const auto& result = work->result;
                if (!result.notice.empty())
                    SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, result.notice.c_str());
                if (!result.success) {
                    MessageBoxW(dialog, result.error.c_str(), L"Gemini APIエラー", MB_OK | MB_ICONWARNING);
                } else if (wParam == 1) {
                    if (!result.notice.empty())
                        SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, result.notice.c_str());
                    ShowResultDialog(dialog, context->instance, result.text);
                } else if (wParam == 2) {
                    std::wstring edited;
                    bool replyAll = false;
                    if (ShowReplyDraftDialog(dialog, context->instance, result.text, edited, replyAll)) {
                        context->mail->replyDraft = std::move(edited);
                        context->mail->insertReplyRequested = true;
                        context->mail->replyAllRequested = replyAll;
                        EndDialog(dialog, IDOK);
                    }
                }
            }
            GeminiAsync::Destroy(work);
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_MASK_ENABLED:
            {
                if (HIWORD(wParam) != BN_CLICKED) return TRUE;
                PiiMaskOptions options;
                std::wstring error;
                if (!LoadPiiMaskOptions(options, error))
                {
                    CheckDlgButton(dialog, IDC_MASK_ENABLED, BST_UNCHECKED);
                    MessageBoxW(dialog, error.c_str(), L"マスキング設定エラー", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const bool previous = options.enabled;
                options.enabled = IsDlgButtonChecked(dialog, IDC_MASK_ENABLED) == BST_CHECKED;
                if (!SavePiiMaskOptions(options, error))
                {
                    CheckDlgButton(dialog, IDC_MASK_ENABLED, previous ? BST_CHECKED : BST_UNCHECKED);
                    MessageBoxW(dialog, error.c_str(), L"マスキング設定エラー", MB_OK | MB_ICONWARNING);
                }
                return TRUE;
            }

            case IDC_AI_SUMMARY:
            {
                auto* context = reinterpret_cast<DialogContext*>(
                    GetWindowLongPtrW(dialog, DWLP_USER));
                if (context == nullptr || context->mail == nullptr || context->busy) return TRUE;
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
                if (masker.IsEnabled() && !ShowPiiMaskPreviewDialog(dialog, context->instance,
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
                const auto& model = context->models[static_cast<size_t>(selected)];
                const std::wstring modelId = model.id, thinking = model.thinking;
                const auto models = context->models;
                GeminiReliabilityOptions reliability;
                if (!LoadGeminiReliabilityOptions(reliability, error)) {
                    MessageBoxW(dialog, error.c_str(), L"設定エラー", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const bool autoFallback = reliability.automaticModelFallback;
                const std::wstring subject = mail.subject, from = mail.from, body = mail.body;
                context->busy = true;
                ShowWindow(GetDlgItem(dialog, IDC_AI_CANCEL_GENERATION), SW_SHOW);
                EnableWindow(GetDlgItem(dialog, IDC_AI_CANCEL_GENERATION), TRUE);
                ShowApiProcessingStatus(dialog, IDC_MAIL_BODY_STATUS,
                    L"Geminiで要約中です。しばらくお待ちください...");
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_GEMINI_MODEL), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_MASK_ENABLED), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_REPLY_TONE), FALSE);
                if (!GeminiAsync::Start(dialog, std::move(apiKey),
                    [modelId, thinking, models, autoFallback, subject, from, body, masker](const std::wstring& key, const std::atomic<int>& cancelState) mutable {
                        return GenerateWithModelFallback(modelId, thinking, models, autoFallback,
                            [&](const std::wstring& candidateId, const std::wstring& candidateThinking) {
                                return GenerateMailSummary(key, candidateId, candidateThinking, subject, from, body, &masker);
                            }, &cancelState);
                    }, 1, &context->work)) {
                    context->busy = false;
                    context->work = nullptr;
                    ShowWindow(GetDlgItem(dialog, IDC_AI_CANCEL_GENERATION), SW_HIDE);
                    EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_GEMINI_MODEL), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_MASK_ENABLED), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_REPLY_TONE), TRUE);
                    SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"バックグラウンド処理を開始できませんでした。");
                    MessageBoxW(dialog, L"非同期処理を開始できませんでした。", L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                }
                return TRUE;
            }

            case IDC_AI_REPLY:
            {
                auto* context = reinterpret_cast<DialogContext*>(
                    GetWindowLongPtrW(dialog, DWLP_USER));
                if (context == nullptr || context->mail == nullptr || context->busy) return TRUE;
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
                const std::wstring tone = GetReplyToneInstruction(static_cast<int>(toneSelected));
                const std::wstring previewRaw = BuildMailReplyPrompt(
                    tone, mail.subject, mail.from, mail.body, L"");
                const std::wstring maskedPreview = masker.Mask(previewRaw);
                if (masker.IsEnabled() && !ShowPiiMaskPreviewDialog(dialog, context->instance,
                    L"返信文案：Gemini送信内容の確認", maskedPreview, masker)) return TRUE;

                std::wstring apiKey, error;
                if (!LoadGeminiApiKey(apiKey, error) || apiKey.empty())
                {
                    MessageBoxW(dialog,
                        error.empty() ? L"先に設定画面でGemini APIキーを保存してください。" : error.c_str(),
                        L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const auto& model = context->models[static_cast<size_t>(selected)];
                const std::wstring modelId = model.id, thinking = model.thinking;
                const auto models = context->models;
                GeminiReliabilityOptions reliability;
                if (!LoadGeminiReliabilityOptions(reliability, error)) {
                    MessageBoxW(dialog, error.c_str(), L"設定エラー", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                const bool autoFallback = reliability.automaticModelFallback;
                const std::wstring subject = mail.subject, from = mail.from, body = mail.body;
                context->busy = true;
                ShowApiProcessingStatus(dialog, IDC_MAIL_BODY_STATUS,
                    L"Geminiで返信文案を作成中です。しばらくお待ちください...");
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_GEMINI_MODEL), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_MASK_ENABLED), FALSE);
                EnableWindow(GetDlgItem(dialog, IDC_REPLY_TONE), FALSE);
                if (!GeminiAsync::Start(dialog, std::move(apiKey),
                    [modelId, thinking, models, autoFallback, tone, subject, from, body, masker](const std::wstring& key, const std::atomic<int>& cancelState) mutable {
                        return GenerateWithModelFallback(modelId, thinking, models, autoFallback,
                            [&](const std::wstring& candidateId, const std::wstring& candidateThinking) {
                                return GenerateMailReplyDraft(key, candidateId, candidateThinking, tone, subject, from, body, L"", &masker);
                            }, &cancelState);
                    }, 2, &context->work)) {
                    context->busy = false;
                    EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_AI_REPLY), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_AI_CONSULT), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_GEMINI_MODEL), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_MASK_ENABLED), TRUE);
                    EnableWindow(GetDlgItem(dialog, IDC_REPLY_TONE), TRUE);
                    SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"バックグラウンド処理を開始できませんでした。");
                    MessageBoxW(dialog, L"非同期処理を開始できませんでした。", L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                }
                return TRUE;
            }

            case IDC_AI_CANCEL_GENERATION:
            {
                auto* context = reinterpret_cast<DialogContext*>(GetWindowLongPtrW(dialog, DWLP_USER));
                if (context && context->busy && context->work) {
                    EnableWindow(GetDlgItem(dialog, IDC_AI_CANCEL_GENERATION), FALSE);
                    SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"キャンセルしています...");
                    GeminiAsync::RequestCancel(context->work);
                }
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
                if (auto* context = reinterpret_cast<DialogContext*>(GetWindowLongPtrW(dialog, DWLP_USER)); context && context->busy) { context->closeAfterCancel = true; if (context->work) GeminiAsync::RequestCancel(context->work); SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"キャンセルして終了しています..."); return TRUE; }
                EndDialog(dialog, LOWORD(wParam));
                return TRUE;

            default:
                break;
            }
            break;

        case WM_CLOSE:
            if (auto* context = reinterpret_cast<DialogContext*>(GetWindowLongPtrW(dialog, DWLP_USER)); context && context->busy) { context->closeAfterCancel = true; if (context->work) GeminiAsync::RequestCancel(context->work); SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"キャンセルして終了しています..."); return TRUE; }
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
