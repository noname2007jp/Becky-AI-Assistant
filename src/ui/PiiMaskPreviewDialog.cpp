#include "PiiMaskPreviewDialog.h"
#include "resource.h"

namespace
{
struct PreviewContext
{
    const std::wstring* title;
    const std::wstring* content;
};

INT_PTR CALLBACK PreviewProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG)
    {
        auto* context = reinterpret_cast<PreviewContext*>(lParam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
        if (context && context->title) SetWindowTextW(dialog, context->title->c_str());
        if (context && context->content)
            SetDlgItemTextW(dialog, IDC_PII_MASK_PREVIEW_TEXT, context->content->c_str());
        return TRUE;
    }
    if (message == WM_COMMAND)
    {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        }
    }
    if (message == WM_CLOSE)
    {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
}

bool ShowPiiMaskPreviewDialog(HWND owner, HINSTANCE instance,
    const std::wstring& title, const std::wstring& maskedContent)
{
    PreviewContext context{ &title, &maskedContent };
    return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_PII_MASK_PREVIEW),
        owner, PreviewProc, reinterpret_cast<LPARAM>(&context)) == IDOK;
}
