#include "SettingsDialog.h"
#include "resource.h"
#include "settings/ApiKeyStore.h"

#include <string>

namespace
{
INT_PTR CALLBACK SettingsProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        std::wstring key, error;
        if (!LoadGeminiApiKey(key, error))
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, error.c_str());
        else if (!key.empty())
            SetDlgItemTextW(dialog, IDC_GEMINI_API_KEY, key.c_str());
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_SETTINGS_SAVE:
        {
            const int length = GetWindowTextLengthW(GetDlgItem(dialog, IDC_GEMINI_API_KEY));
            if (length <= 0 || length > 4096)
            {
                SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, L"APIキーを入力してください。");
                return TRUE;
            }
            std::wstring key(static_cast<size_t>(length) + 1, L'\0');
            GetDlgItemTextW(dialog, IDC_GEMINI_API_KEY, key.data(), length + 1);
            key.resize(static_cast<size_t>(length));
            std::wstring error;
            if (SaveGeminiApiKey(key, error))
            {
                SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS,
                                L"APIキーをWindows DPAPIで暗号化して保存しました。");
                SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
            }
            else
            {
                SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, error.c_str());
            }
            return TRUE;
        }
        case IDCANCEL:
        case IDOK:
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
}

INT_PTR ShowSettingsDialog(HWND owner, HINSTANCE instance)
{
    return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_AI_SETTINGS),
                           owner, SettingsProc, 0);
}
