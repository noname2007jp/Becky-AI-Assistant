#pragma once
#include "LauncherDialog.h"
#include <windows.h>
#include <string>

INT_PTR ShowConsultDialog(HWND owner, HINSTANCE instance, MailContext& mail,
    const std::wstring& modelId, const std::wstring& thinkingLevel);
