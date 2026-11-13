#include "ResultDialog.h"
#include "resource.h"

#include <cstring>

namespace
{
    struct ResultDialogContext
    {
        const std::wstring* result;
    };

    bool CopyToClipboard(HWND owner, const std::wstring& text)
    {
        if (!OpenClipboard(owner))
            return false;

        if (!EmptyClipboard())
        {
            CloseClipboard();
            return false;
        }

        const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory == nullptr)
        {
            CloseClipboard();
            return false;
        }

        void* destination = GlobalLock(memory);
        if (destination == nullptr)
        {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }

        std::memcpy(destination, text.c_str(), bytes);
        GlobalUnlock(memory);

        if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr)
        {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }

        // Clipboard owns the HGLOBAL after SetClipboardData succeeds.
        CloseClipboard();
        return true;
    }

    INT_PTR CALLBACK ResultDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
        {
            auto* context = reinterpret_cast<ResultDialogContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
            if (context != nullptr && context->result != nullptr)
                SetDlgItemTextW(dialog, IDC_AI_RESULT_TEXT, context->result->c_str());

            SendDlgItemMessageW(dialog, IDC_AI_RESULT_TEXT, EM_SETSEL, 0, 0);
            SetDlgItemTextW(dialog, IDC_AI_COPY_STATUS,
                L"必要な箇所を選択して Ctrl+C でもコピーできます。");
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_AI_COPY_ALL:
            {
                auto* context = reinterpret_cast<ResultDialogContext*>(
                    GetWindowLongPtrW(dialog, DWLP_USER));
                if (context != nullptr && context->result != nullptr &&
                    CopyToClipboard(dialog, *context->result))
                {
                    SetDlgItemTextW(dialog, IDC_AI_COPY_STATUS,
                        L"要約全文をクリップボードにコピーしました。");
                }
                else
                {
                    SetDlgItemTextW(dialog, IDC_AI_COPY_STATUS,
                        L"コピーできませんでした。もう一度お試しください。");
                }
                return TRUE;
            }

            case IDC_AI_RESULT_CLOSE:
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

INT_PTR ShowResultDialog(HWND owner, HINSTANCE instance, const std::wstring& result)
{
    ResultDialogContext context{ &result };
    return DialogBoxParamW(
        instance,
        MAKEINTRESOURCEW(IDD_AI_RESULT),
        owner,
        ResultDialogProc,
        reinterpret_cast<LPARAM>(&context));
}
