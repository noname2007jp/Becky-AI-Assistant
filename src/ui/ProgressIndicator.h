#pragma once

#include <windows.h>

// Paint the status before a synchronous Gemini request blocks the dialog thread.
inline void ShowApiProcessingStatus(HWND dialog, int statusControlId, const wchar_t* message)
{
    SetDlgItemTextW(dialog, statusControlId, message);
    HWND status = GetDlgItem(dialog, statusControlId);
    if (status != nullptr)
        RedrawWindow(status, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    RedrawWindow(dialog, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}
