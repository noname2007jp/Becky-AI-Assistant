#pragma once
#include <windows.h>
#include <string>
#include "gemini/PiiMasker.h"

bool ShowPiiMaskPreviewDialog(HWND owner, HINSTANCE instance,
    const std::wstring& title, const std::wstring& maskedContent, PiiMasker& masker);
