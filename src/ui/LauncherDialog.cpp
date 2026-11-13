#include "LauncherDialog.h"
#include "resource.h"

namespace
{
    INT_PTR CALLBACK LauncherDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
        {
            const auto* mail = reinterpret_cast<const MailContext*>(lParam);
            if (mail != nullptr)
            {
                SetDlgItemTextW(dialog, IDC_MAIL_SUBJECT,
                    mail->subject.empty() ? L"(件名を取得できません)" : mail->subject.c_str());
                SetDlgItemTextW(dialog, IDC_MAIL_FROM,
                    mail->from.empty() ? L"(差出人を取得できません)" : mail->from.c_str());
            }
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_AI_SUMMARY:
            case IDC_AI_REPLY:
            case IDC_AI_CONSULT:
            case IDC_AI_SETTINGS:
                MessageBoxW(
                    dialog,
                    L"この機能は現在開発中です。次の更新で利用できるようになります。",
                    L"Becky AI Assistant",
                    MB_OK | MB_ICONINFORMATION);
                return TRUE;

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
    return DialogBoxParamW(
        instance,
        MAKEINTRESOURCEW(IDD_AI_LAUNCHER),
        owner,
        LauncherDialogProc,
        reinterpret_cast<LPARAM>(&mail));
}
