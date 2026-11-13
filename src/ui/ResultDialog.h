#pragma once

#include <windows.h>
#include <string>

INT_PTR ShowResultDialog(HWND owner, HINSTANCE instance, const std::wstring& result);
