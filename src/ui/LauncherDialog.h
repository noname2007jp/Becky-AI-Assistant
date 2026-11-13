#pragma once

#include <windows.h>
#include <string>

struct MailContext
{
    std::wstring mailId;
    std::wstring from;
    std::wstring subject;
};

INT_PTR ShowLauncherDialog(HWND owner, HINSTANCE instance, const MailContext& mail);
