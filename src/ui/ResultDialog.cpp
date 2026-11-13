#include "ResultDialog.h"
#include "resource.h"

#include <richedit.h>
#include <cstring>
#include <vector>

namespace
{
    struct FormatRange { LONG start; LONG length; };
    struct ResultDialogContext { const std::wstring* result; };

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

    std::wstring MakeDisplayText(const std::wstring& source, std::vector<FormatRange>& boldRanges)
    {
        std::wstring output;
        size_t pos = 0;
        while (pos < source.size())
        {
            size_t end = source.find(L'\n', pos);
            if (end == std::wstring::npos) end = source.size();
            size_t lineStart = output.size();
            size_t p = pos;
            while (p < end && (source[p] == L' ' || source[p] == L'\t')) ++p;
            bool bullet = p + 1 < end &&
                ((source[p] == L'*' || source[p] == L'-') && source[p + 1] == L' ');
            if (bullet)
            {
                output.append(source, pos, p - pos);
                output += L"・ ";
                p += 2;
            }

            bool bold = false;
            LONG boldStart = -1;
            while (p < end)
            {
                if (p + 1 < end && source[p] == L'*' && source[p + 1] == L'*')
                {
                    if (!bold)
                    {
                        bold = true;
                        boldStart = static_cast<LONG>(output.size());
                    }
                    else
                    {
                        LONG length = static_cast<LONG>(output.size()) - boldStart;
                        if (length > 0) boldRanges.push_back({ boldStart, length });
                        bold = false;
                        boldStart = -1;
                    }
                    p += 2;
                    continue;
                }
                output += source[p++];
            }
            if (bold && boldStart >= 0)
            {
                LONG length = static_cast<LONG>(output.size()) - boldStart;
                if (length > 0) boldRanges.push_back({ boldStart, length });
            }
            if (end < source.size()) output += L'\n';
            pos = end + (end < source.size() ? 1 : 0);
        }
        return output;
    }

    void SetFormattedResult(HWND edit, const std::wstring& source)
    {
        std::vector<FormatRange> ranges;
        const std::wstring display = MakeDisplayText(source, ranges);
        SetWindowTextW(edit, display.c_str());

        CHARFORMAT2W format{};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_BOLD;
        format.dwEffects = CFE_BOLD;
        for (const auto& range : ranges)
        {
            SendMessageW(edit, EM_SETSEL, range.start, range.start + range.length);
            SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION,
                reinterpret_cast<LPARAM>(&format));
        }
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
            HWND resultEdit = GetDlgItem(dialog, IDC_AI_RESULT_TEXT);
            if (context && context->result) SetFormattedResult(resultEdit, *context->result);
            SendMessageW(resultEdit, EM_SETSEL, 0, 0);
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
                if (context && context->result && CopyToClipboard(dialog, *context->result))
                    SetDlgItemTextW(dialog, IDC_AI_COPY_STATUS, L"要約全文をクリップボードにコピーしました。");
                else
                    SetDlgItemTextW(dialog, IDC_AI_COPY_STATUS, L"コピーできませんでした。もう一度お試しください。");
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

INT_PTR ShowResultDialog(HWND owner, HINSTANCE instance, const std::wstring& result)
{
    // Load the Windows Rich Edit control before creating the dialog.
    LoadLibraryW(L"Msftedit.dll");
    ResultDialogContext context{ &result };
    return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_AI_RESULT),
        owner, ResultDialogProc, reinterpret_cast<LPARAM>(&context));
}
