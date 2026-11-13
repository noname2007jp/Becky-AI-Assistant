#pragma once
#include <windows.h>
#include <string>

INT_PTR ShowResultDialog(HWND owner, HINSTANCE instance, const std::wstring& result,
    const wchar_t* title = L"Geminiによるメール要約");
bool ShowReplyDraftDialog(HWND owner, HINSTANCE instance,
    const std::wstring& draft, std::wstring& edited);
