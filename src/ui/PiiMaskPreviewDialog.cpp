#include "PiiMaskPreviewDialog.h"
#include "resource.h"

namespace
{
// Win32 multiline EDIT controls require CRLF. API prompt text intentionally
// keeps its original LF line endings; normalize only the preview copy.
std::wstring ToEditControlLineEndings(const std::wstring& text)
{
    std::wstring normalized;
    normalized.reserve(text.size() + 16);
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == L'\r')
        {
            normalized += L"\r\n";
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
        }
        else if (text[i] == L'\n')
        {
            normalized += L"\r\n";
        }
        else
        {
            normalized += text[i];
        }
    }
    return normalized;
}

struct PreviewContext
{
    const std::wstring* title;
    std::wstring* content;
    PiiMasker* masker;
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
        if (LOWORD(wParam) == IDC_PII_MASK_SELECTION)
        {
            auto* context = reinterpret_cast<PreviewContext*>(GetWindowLongPtrW(dialog, DWLP_USER));
            HWND edit = GetDlgItem(dialog, IDC_PII_MASK_PREVIEW_TEXT);
            DWORD start = 0, end = 0;
            SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
            if (!context || !context->content || !context->masker || start == end)
            {
                MessageBoxW(dialog, L"先にマスクしたい文字列をマウスで選択してください。", L"手動マスキング", MB_OK | MB_ICONINFORMATION);
                return TRUE;
            }
            if (!context->masker->MaskSelected(*context->content, start, end - start))
            {
                MessageBoxW(dialog, L"選択範囲をマスクできません。別の文字列を選択してください。", L"手動マスキング", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            SetDlgItemTextW(dialog, IDC_PII_MASK_PREVIEW_TEXT, context->content->c_str());
            return TRUE;
        }
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
    const std::wstring& title, const std::wstring& maskedContent, PiiMasker& masker)
{
    std::wstring editablePreview = ToEditControlLineEndings(maskedContent);
    PreviewContext context{ &title, &editablePreview, &masker };
    return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_PII_MASK_PREVIEW),
        owner, PreviewProc, reinterpret_cast<LPARAM>(&context)) == IDOK;
}
