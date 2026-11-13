#include "SettingsDialog.h"
#include "resource.h"
#include "settings/ApiKeyStore.h"
#include "gemini/GeminiConnection.h"

#include <string>
#include <commctrl.h>
#include <shellapi.h>

namespace
{
constexpr UINT WM_GEMINI_TEST_FINISHED = WM_APP + 41;

struct SettingsState
{
    bool testRunning = false;
};

struct TestResult
{
    GeminiConnectionResult result;
};

DWORD WINAPI GeminiTestThread(void* parameter)
{
    HWND dialog = static_cast<HWND>(parameter);
    auto* result = new TestResult{};
    std::wstring key, error;
    if (!LoadGeminiApiKey(key, error))
    {
        result->result.status = GeminiConnectionStatus::InvalidApiKey;
        result->result.message = error;
    }
    else
    {
        result->result = TestGeminiConnection(key);
        if (!key.empty()) SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
    }

    if (!PostMessageW(dialog, WM_GEMINI_TEST_FINISHED, 0,
                      reinterpret_cast<LPARAM>(result)))
        delete result;
    return 0;
}

INT_PTR CALLBACK SettingsProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        auto* state = reinterpret_cast<SettingsState*>(lParam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
        std::wstring key, error;
        SetDlgItemTextW(dialog, IDC_GEMINI_APIKEY_LINK,
            L"<a href=\"https://aistudio.google.com/api-keys\">Google AI StudioでAPIキーを取得</a>");
        if (!LoadGeminiApiKey(key, error))
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, error.c_str());
        else if (!key.empty())
            SetDlgItemTextW(dialog, IDC_GEMINI_API_KEY, key.c_str());
        if (!key.empty()) SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
        return TRUE;
    }

    case WM_NOTIFY:
    {
        const auto* notification = reinterpret_cast<const NMHDR*>(lParam);
        if (notification != nullptr &&
            notification->idFrom == IDC_GEMINI_APIKEY_LINK &&
            (notification->code == NM_CLICK || notification->code == NM_RETURN))
        {
            ShellExecuteW(dialog, L"open",
                L"https://aistudio.google.com/api-keys",
                nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        break;
    }

    case WM_GEMINI_TEST_FINISHED:
    {
        auto* state = reinterpret_cast<SettingsState*>(
            GetWindowLongPtrW(dialog, DWLP_USER));
        auto* result = reinterpret_cast<TestResult*>(lParam);
        if (result != nullptr)
        {
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, result->result.message.c_str());
            delete result;
        }
        if (state != nullptr) state->testRunning = false;
        EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_TEST), TRUE);
        EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), TRUE);
        EnableWindow(GetDlgItem(dialog, IDCANCEL), TRUE);
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
            const bool saved = SaveGeminiApiKey(key, error);
            SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS,
                saved ? L"APIキーを暗号化して保存しました。「接続テスト」で確認できます。"
                      : error.c_str());
            return TRUE;
        }

        case IDC_SETTINGS_TEST:
        {
            auto* state = reinterpret_cast<SettingsState*>(
                GetWindowLongPtrW(dialog, DWLP_USER));
            if (state == nullptr || state->testRunning) return TRUE;

            state->testRunning = true;
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS,
                            L"Gemini APIへ接続しています...");
            EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_TEST), FALSE);
            EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), FALSE);
            EnableWindow(GetDlgItem(dialog, IDCANCEL), FALSE);

            HANDLE thread = CreateThread(nullptr, 0, GeminiTestThread, dialog, 0, nullptr);
            if (thread == nullptr)
            {
                state->testRunning = false;
                EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_TEST), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), TRUE);
                EnableWindow(GetDlgItem(dialog, IDCANCEL), TRUE);
                SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS,
                                L"接続テストを開始できませんでした。");
            }
            else
            {
                CloseHandle(thread);
            }
            return TRUE;
        }

        case IDCANCEL:
        case IDOK:
        {
            auto* state = reinterpret_cast<SettingsState*>(
                GetWindowLongPtrW(dialog, DWLP_USER));
            if (state != nullptr && state->testRunning) return TRUE;
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        }
        }
        break;

    case WM_CLOSE:
    {
        auto* state = reinterpret_cast<SettingsState*>(
            GetWindowLongPtrW(dialog, DWLP_USER));
        if (state != nullptr && state->testRunning) return TRUE;
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    }
    return FALSE;
}
}

INT_PTR ShowSettingsDialog(HWND owner, HINSTANCE instance)
{
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LINK_CLASS;
    InitCommonControlsEx(&controls);

    SettingsState state;
    return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_AI_SETTINGS),
                           owner, SettingsProc, reinterpret_cast<LPARAM>(&state));
}
