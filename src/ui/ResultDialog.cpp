#include "ResultDialog.h"
#include "resource.h"
#include "MarkdownRenderer.h"

#include <richedit.h>
#include <cstring>
#include <vector>
#include <utility>

namespace
{
    struct ResultDialogContext { const std::wstring* result; std::wstring* edited; const wchar_t* title; bool replyMode; bool* replyAll; };

    bool CopyToClipboard(HWND owner, const std::wstring& text)
    {
        if (!OpenClipboard(owner)) return false;
        if (!EmptyClipboard()) { CloseClipboard(); return false; }
        const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory) { CloseClipboard(); return false; }
        void* destination = GlobalLock(memory);
        if (!destination) { GlobalFree(memory); CloseClipboard(); return false; }
        std::memcpy(destination, text.c_str(), bytes);
        GlobalUnlock(memory);
        if (!SetClipboardData(CF_UNICODETEXT, memory))
        {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }
        CloseClipboard();
        return true;
    }

    void SetFormattedResult(HWND edit, const std::wstring& source)
    {
        // RichEdit uses CR as its paragraph separator. MarkdownRenderer normalizes
        // CRLF/LF and calculates formatting ranges against the rendered text.
        const MarkdownRenderer::Result rendered = MarkdownRenderer::Render(source);
        SetWindowTextW(edit, rendered.text.c_str());

        CHARFORMAT2W format{};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_BOLD;
        format.dwEffects = CFE_BOLD;
        const auto applyBold = [&](const MarkdownRenderer::Range& range) {
            if (range.length == 0 ||
                range.start > static_cast<size_t>(LONG_MAX) ||
                range.length > static_cast<size_t>(LONG_MAX) ||
                range.start + range.length > rendered.text.size())
                return;
            SendMessageW(edit, EM_SETSEL,
                static_cast<WPARAM>(range.start),
                static_cast<LPARAM>(range.start + range.length));
            SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION,
                reinterpret_cast<LPARAM>(&format));
        };
        for (const auto& range : rendered.bold) applyBold(range);
        for (const auto& range : rendered.headings) applyBold(range);
        SendMessageW(edit, EM_SETSEL, 0, 0);
    }

    INT_PTR CALLBACK ResultDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_INITDIALOG:
        {
            auto* context = reinterpret_cast<ResultDialogContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
            if (context && context->title) SetWindowTextW(dialog, context->title);
            HWND resultEdit = GetDlgItem(dialog, IDC_AI_RESULT_TEXT);
            if (context && context->result) {
                if (context->replyMode) SetWindowTextW(resultEdit, context->result->c_str());
                else SetFormattedResult(resultEdit, *context->result);
            }
            if (context && context->replyMode) {
                SendMessageW(resultEdit, EM_SETREADONLY, FALSE, 0);
                SetDlgItemTextW(dialog, IDC_AI_COPY_STATUS, L"返信内容を編集し、確認後に返信画面へ挿入してください。");
                ShowWindow(GetDlgItem(dialog, IDC_AI_INSERT_REPLY), SW_SHOW);
                ShowWindow(GetDlgItem(dialog, IDC_AI_INSERT_REPLY_ALL), SW_SHOW);
            } else {
                SendMessageW(resultEdit, EM_SETREADONLY, TRUE, 0);
                SetDlgItemTextW(dialog, IDC_AI_COPY_STATUS, L"必要な箇所を選択して Ctrl+C でもコピーできます。");
                ShowWindow(GetDlgItem(dialog, IDC_AI_INSERT_REPLY), SW_HIDE);
                ShowWindow(GetDlgItem(dialog, IDC_AI_INSERT_REPLY_ALL), SW_HIDE);
            }
            SendMessageW(resultEdit, EM_SETSEL, 0, 0);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDC_AI_COPY_ALL:
            {
                auto* context = reinterpret_cast<ResultDialogContext*>(
                    GetWindowLongPtrW(dialog, DWLP_USER));
                if (context && context->result) {
                    HWND edit = GetDlgItem(dialog, IDC_AI_RESULT_TEXT);
                    const int length = GetWindowTextLengthW(edit);
                    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
                    GetWindowTextW(edit, value.data(), length + 1);
                    value.resize(static_cast<size_t>(length));
                    if (CopyToClipboard(dialog, context->replyMode ? value : *context->result))
                        SetDlgItemTextW(dialog, IDC_AI_COPY_STATUS, L"全文をクリップボードにコピーしました。");
                    else
                        SetDlgItemTextW(dialog, IDC_AI_COPY_STATUS, L"コピーできませんでした。もう一度お試しください。");
                }
                return TRUE;
            }
            case IDC_AI_INSERT_REPLY:
            case IDC_AI_INSERT_REPLY_ALL:
            {
                auto* context = reinterpret_cast<ResultDialogContext*>(GetWindowLongPtrW(dialog, DWLP_USER));
                if (!context || !context->replyMode || !context->edited) return TRUE;
                HWND edit = GetDlgItem(dialog, IDC_AI_RESULT_TEXT);
                const int length = GetWindowTextLengthW(edit);
                std::wstring value(static_cast<size_t>(length) + 1, L'\0');
                GetWindowTextW(edit, value.data(), length + 1);
                value.resize(static_cast<size_t>(length));
                if (value.empty()) {
                    MessageBoxW(dialog, L"返信文が空です。", L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
                    return TRUE;
                }
                *context->edited = std::move(value);
                if (context->replyAll) *context->replyAll = (LOWORD(wParam) == IDC_AI_INSERT_REPLY_ALL);
                EndDialog(dialog, LOWORD(wParam));
                return TRUE;
            }
            case IDC_AI_RESULT_CLOSE:
            case IDCANCEL:
            case IDOK:
                EndDialog(dialog, LOWORD(wParam));
                return TRUE;
            default: break;
            }
            break;
        case WM_CLOSE:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        default: break;
        }
        return FALSE;
    }
}

INT_PTR ShowResultDialog(HWND owner, HINSTANCE instance, const std::wstring& result, const wchar_t* title)
{
    // Load the Windows Rich Edit control before creating the dialog.
    LoadLibraryW(L"Msftedit.dll");
    ResultDialogContext context{ &result, nullptr, title, false, nullptr };
    return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_AI_RESULT),
        owner, ResultDialogProc, reinterpret_cast<LPARAM>(&context));
}

bool ShowReplyDraftDialog(HWND owner, HINSTANCE instance, const std::wstring& draft, std::wstring& edited, bool& replyAll, const wchar_t* title)
{
    LoadLibraryW(L"Msftedit.dll");
    replyAll = false;
    ResultDialogContext context{ &draft, &edited, title ? title : L"返信文案", true, &replyAll };
    const INT_PTR result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_AI_RESULT), owner,
        ResultDialogProc, reinterpret_cast<LPARAM>(&context));
    return result == IDC_AI_INSERT_REPLY || result == IDC_AI_INSERT_REPLY_ALL;
}
