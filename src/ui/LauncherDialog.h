#pragma once

#include <windows.h>
#include <string>

struct MailContext
{
    std::wstring mailId;
    std::wstring from;
    std::wstring subject;
    std::wstring body;
    std::wstring bodyStatus;
    std::wstring replyDraft;
    bool insertReplyRequested = false;
    bool replyAllRequested = false;
};

INT_PTR ShowLauncherDialog(HWND owner, HINSTANCE instance, MailContext& mail);
