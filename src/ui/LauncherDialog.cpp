#include "LauncherDialog.h"
#include "SettingsDialog.h"
#include "resource.h"
#include "settings/ApiKeyStore.h"
#include "gemini/GeminiGenerate.h"

namespace
{
    struct DialogContext
    {
        const MailContext* mail;
        HINSTANCE instance;
    };

    INT_PTR CALLBACK LauncherDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
        {
            auto* context = reinterpret_cast<DialogContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
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
                    L"次の情報をGoogle Gemini APIへ送信して要約します。\\n\\n"
                    L"・メールの件名\\n・差出人\\n・メール本文\\n\\n"
                    L"メール本文には機密情報が含まれる可能性があります。"
                    L"送信先と内容を確認し、許可する場合のみ「はい」を選択してください。"
                    L"キャンセルすると外部送信しません。",
                    L"メール本文の外部送信確認",
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
                    L"Geminiで要約中です。しばらくお待ちください...");
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), FALSE);
                GeminiGenerateResult result = GenerateMailSummary(
                    apiKey, mail.subject, mail.from, mail.body);
                if (!apiKey.empty()) SecureZeroMemory(apiKey.data(), apiKey.size() * sizeof(wchar_t));
                EnableWindow(GetDlgItem(dialog, IDC_AI_SUMMARY), TRUE);
                SetDlgItemTextW(dialog, IDC_MAIL_BODY_STATUS, L"");
                if (result.success)
                    MessageBoxW(dialog, result.text.c_str(),
                        L"Geminiによるメール要約", MB_OK | MB_ICONINFORMATION);
                else
                    MessageBoxW(dialog, result.error.c_str(),
                        L"Gemini APIエラー", MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            case IDC_AI_REPLY:
            case IDC_AI_CONSULT:
                MessageBoxW(dialog,
                    L"この機能は次の開発段階で対応します。",
                    L"Becky AI Assistant", MB_OK | MB_ICONINFORMATION);
                return TRUE;

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

INT_PTR ShowLauncherDialog(HWND owner, HINSTANCE instance, const MailContext& mail)
{
    DialogContext context{ &mail, instance };
    return DialogBoxParamW(
        instance,
        MAKEINTRESOURCEW(IDD_AI_LAUNCHER),
        owner,
        LauncherDialogProc,
        reinterpret_cast<LPARAM>(&context));
}
