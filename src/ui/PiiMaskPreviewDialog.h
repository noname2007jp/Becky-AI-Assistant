#pragma once
#include <windows.h>
#include <string>

bool ShowPiiMaskPreviewDialog(HWND owner, HINSTANCE instance,
    const std::wstring& title, const std::wstring& maskedContent);
