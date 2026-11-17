#include "SettingsDialog.h"
#include "resource.h"
#include "ProgressIndicator.h"
#include "settings/ApiKeyStore.h"
#include "gemini/GeminiConnection.h"
#include "gemini/GeminiModels.h"

#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <commctrl.h>
#include <shellapi.h>

namespace
{
constexpr UINT WM_GEMINI_TEST_FINISHED = WM_APP + 41;
constexpr UINT WM_MODEL_UPDATE_FINISHED = WM_APP + 42;

struct SettingsState
{
    bool testRunning = false;
    bool apiKeyVisible = false;
    bool modelUpdateRunning = false;
    std::vector<GeminiModelOption> models;
};

void PopulateDefaultModelCombo(HWND dialog, SettingsState* state, const std::wstring& preferred)
{
    if (!state) return;
    state->models.clear();
    std::wstring error;
    if (!LoadGeminiModelCatalog(state->models, error) || state->models.empty())
        state->models = DefaultGeminiModels();
    HWND combo = GetDlgItem(dialog, IDC_SETTINGS_DEFAULT_MODEL);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    int selected = -1;
    for (size_t i = 0; i < state->models.size(); ++i)
    {
        SendMessageW(combo, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(state->models[i].label.c_str()));
        if (state->models[i].id == preferred) selected = static_cast<int>(i);
    }
    if (selected < 0)
    {
        for (size_t i = 0; i < state->models.size(); ++i)
            if (state->models[i].id == L"gemini-3.5-flash-lite")
                selected = static_cast<int>(i);
    }
    if (selected < 0 && !state->models.empty()) selected = 0;
    if (selected >= 0) SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

struct TestResult
{
    GeminiConnectionResult result;
};
struct ModelUpdateResult
{
    bool success = false;
    std::wstring message;
    std::vector<GeminiModelOption> models;
};

struct ModelUpdateArgs
{
    HWND dialog;
    bool includePaid;
};

DWORD WINAPI ModelUpdateThread(void* parameter)
{
    std::unique_ptr<ModelUpdateArgs> args(static_cast<ModelUpdateArgs*>(parameter));
    auto* result = new ModelUpdateResult{};
    std::wstring key, error;
    if (!LoadGeminiApiKey(key, error) || key.empty())
    {
        result->message = error.empty() ? L"先にGemini APIキーを保存してください。" : error;
    }
    else
    {
        result->success = FetchGeminiModelCatalog(key, args->includePaid, result->models, error);
        SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
        if (result->success)
        {
            if (!SaveGeminiModelCatalog(result->models, error))
                result->success = false;
        }
        result->message = result->success
            ? L"Google公式料金ページを照合し、モデル一覧を更新しました（" + std::to_wstring(result->models.size()) + L"件）。"
            : error;
    }
    if (!PostMessageW(args->dialog, WM_MODEL_UPDATE_FINISHED, 0, reinterpret_cast<LPARAM>(result)))
        delete result;
    return 0;
}

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
        SetDlgItemTextW(dialog, IDC_SETTINGS_PRICING_LINK,
            L"<a href=\"https://aistudio.google.com/docs/pricing\">Google公式料金ページを開く</a>");
        if (!LoadGeminiApiKey(key, error))
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, error.c_str());
        else if (!key.empty())
            SetDlgItemTextW(dialog, IDC_GEMINI_API_KEY, key.c_str());
        if (!key.empty()) SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
        std::vector<std::wstring> maskTerms;
        std::wstring maskError;
        if (LoadPiiMaskTerms(maskTerms, maskError))
        {
            std::wstring text;
            for (const auto& term : maskTerms) text += term + L"\r\n";
            SetDlgItemTextW(dialog, IDC_SETTINGS_MASK_TERMS, text.c_str());
        }
        else if (!maskError.empty())
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, maskError.c_str());
        PiiMaskOptions maskOptions;
        std::wstring optionsError;
        if (LoadPiiMaskOptions(maskOptions, optionsError))
        {
            CheckDlgButton(dialog, IDC_MASK_EMAIL, maskOptions.email ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_MASK_PHONE, maskOptions.phone ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_MASK_POSTAL, maskOptions.postal ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_MASK_IP, maskOptions.ip ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_MASK_COMPANY, maskOptions.company ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_MASK_PERSON, maskOptions.person ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_MASK_ADDRESS, maskOptions.address ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_MASK_ACCOUNT, maskOptions.account ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_MASK_CUSTOM, maskOptions.custom ? BST_CHECKED : BST_UNCHECKED);
        }
        else SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, optionsError.c_str());
        CheckDlgButton(dialog, IDC_SETTINGS_INCLUDE_PAID, BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_SETTINGS_AUTO_FALLBACK, BST_CHECKED);
        GeminiReliabilityOptions reliability;
        std::wstring reliabilityError;
        if (LoadGeminiReliabilityOptions(reliability, reliabilityError))
        {
            CheckDlgButton(dialog, IDC_SETTINGS_AUTO_FALLBACK,
                reliability.automaticModelFallback ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_SETTINGS_DEBUG_LOG,
                reliability.debugLogging ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, reliabilityError.c_str());
        PopulateDefaultModelCombo(dialog, state, reliability.defaultModel);
        return TRUE;
    }

    case WM_NOTIFY:
    {
        const auto* notification = reinterpret_cast<const NMHDR*>(lParam);
        if (notification != nullptr &&
            notification->idFrom == IDC_SETTINGS_PRICING_LINK &&
            (notification->code == NM_CLICK || notification->code == NM_RETURN))
        {
            ShellExecuteW(dialog, L"open", L"https://aistudio.google.com/docs/pricing", nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
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

    case WM_MODEL_UPDATE_FINISHED:
    {
        auto* state = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(dialog, DWLP_USER));
        auto* result = reinterpret_cast<ModelUpdateResult*>(lParam);
        if (result)
        {
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, result->message.c_str());
            if (result->success && state)
            {
                std::wstring preferred = L"gemini-3.5-flash-lite";
                const LRESULT previous = SendDlgItemMessageW(dialog, IDC_SETTINGS_DEFAULT_MODEL, CB_GETCURSEL, 0, 0);
                if (previous >= 0 && previous < static_cast<LRESULT>(state->models.size()))
                    preferred = state->models[static_cast<size_t>(previous)].id;
                state->models = std::move(result->models);
                PopulateDefaultModelCombo(dialog, state, preferred);
            }
            delete result;
        }
        if (state) state->modelUpdateRunning = false;
        EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_UPDATE_MODELS), TRUE);
        EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_TEST), TRUE);
        EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_UPDATE_MODELS), TRUE);
        EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), TRUE);
        EnableWindow(GetDlgItem(dialog, IDCANCEL), TRUE);
        return TRUE;
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
        EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_UPDATE_MODELS), TRUE);
        EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), TRUE);
        EnableWindow(GetDlgItem(dialog, IDCANCEL), TRUE);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_SETTINGS_TOGGLE_KEY:
        {
            auto* state = reinterpret_cast<SettingsState*>(
                GetWindowLongPtrW(dialog, DWLP_USER));
            if (state == nullptr) return TRUE;

            HWND edit = GetDlgItem(dialog, IDC_GEMINI_API_KEY);
            state->apiKeyVisible = !state->apiKeyVisible;
            SendMessageW(edit, EM_SETPASSWORDCHAR,
                         state->apiKeyVisible ? 0 : L'*', 0);
            InvalidateRect(edit, nullptr, TRUE);
            SetDlgItemTextW(dialog, IDC_SETTINGS_TOGGLE_KEY,
                            state->apiKeyVisible ? L"非表示" : L"表示");
            return TRUE;
        }

        case IDC_SETTINGS_SAVE:
        {
            const int keyLength = GetWindowTextLengthW(GetDlgItem(dialog, IDC_GEMINI_API_KEY));
            std::wstring error;
            bool keySaved = true;
            if (keyLength > 0)
            {
                if (keyLength > 4096)
                {
                    SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, L"APIキーが長すぎます。");
                    return TRUE;
                }
                std::wstring key(static_cast<size_t>(keyLength) + 1, L'\0');
                GetDlgItemTextW(dialog, IDC_GEMINI_API_KEY, key.data(), keyLength + 1);
                key.resize(static_cast<size_t>(keyLength));
                keySaved = SaveGeminiApiKey(key, error);
                SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
            }
            const int termsLength = GetWindowTextLengthW(GetDlgItem(dialog, IDC_SETTINGS_MASK_TERMS));
            if (termsLength > 65536)
            {
                SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, L"マスキング語句が多すぎます。");
                return TRUE;
            }
            std::wstring raw(static_cast<size_t>(termsLength) + 1, L'\0');
            GetDlgItemTextW(dialog, IDC_SETTINGS_MASK_TERMS, raw.data(), termsLength + 1);
            raw.resize(static_cast<size_t>(termsLength));
            std::vector<std::wstring> terms;
            size_t lineStart = 0;
            while (lineStart < raw.size())
            {
                size_t lineEnd = raw.find_first_of(L"\r\n", lineStart);
                if (lineEnd == std::wstring::npos) lineEnd = raw.size();
                std::wstring term = raw.substr(lineStart, lineEnd - lineStart);
                if (term.size() >= 2) terms.push_back(std::move(term));
                lineStart = lineEnd + 1;
                while (lineStart < raw.size() && (raw[lineStart] == L'\r' || raw[lineStart] == L'\n')) ++lineStart;
            }
            const bool termsSaved = SavePiiMaskTerms(terms, error);
            PiiMaskOptions maskOptions;
            if (!LoadPiiMaskOptions(maskOptions, error))
            {
                SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, error.c_str());
                return TRUE;
            }
            maskOptions.email = IsDlgButtonChecked(dialog, IDC_MASK_EMAIL) == BST_CHECKED;
            maskOptions.phone = IsDlgButtonChecked(dialog, IDC_MASK_PHONE) == BST_CHECKED;
            maskOptions.postal = IsDlgButtonChecked(dialog, IDC_MASK_POSTAL) == BST_CHECKED;
            maskOptions.ip = IsDlgButtonChecked(dialog, IDC_MASK_IP) == BST_CHECKED;
            maskOptions.company = IsDlgButtonChecked(dialog, IDC_MASK_COMPANY) == BST_CHECKED;
            maskOptions.person = IsDlgButtonChecked(dialog, IDC_MASK_PERSON) == BST_CHECKED;
            maskOptions.address = IsDlgButtonChecked(dialog, IDC_MASK_ADDRESS) == BST_CHECKED;
            maskOptions.account = IsDlgButtonChecked(dialog, IDC_MASK_ACCOUNT) == BST_CHECKED;
            maskOptions.custom = IsDlgButtonChecked(dialog, IDC_MASK_CUSTOM) == BST_CHECKED;
            const bool optionsSaved = SavePiiMaskOptions(maskOptions, error);
            GeminiReliabilityOptions reliability;
            reliability.automaticModelFallback =
                IsDlgButtonChecked(dialog, IDC_SETTINGS_AUTO_FALLBACK) == BST_CHECKED;
            reliability.debugLogging =
                IsDlgButtonChecked(dialog, IDC_SETTINGS_DEBUG_LOG) == BST_CHECKED;
            auto* state = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(dialog, DWLP_USER));
            const LRESULT modelIndex = SendDlgItemMessageW(dialog, IDC_SETTINGS_DEFAULT_MODEL, CB_GETCURSEL, 0, 0);
            if (state && modelIndex >= 0 && modelIndex < static_cast<LRESULT>(state->models.size()))
                reliability.defaultModel = state->models[static_cast<size_t>(modelIndex)].id;
            const bool reliabilitySaved = SaveGeminiReliabilityOptions(reliability, error);
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS,
                keySaved && termsSaved && optionsSaved && reliabilitySaved
                    ? L"設定を暗号化して保存しました。"
                    : (error.empty() ? L"設定を保存できませんでした。" : error.c_str()));
            return TRUE;
        }

        case IDC_SETTINGS_UPDATE_MODELS:
        {
            auto* state = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(dialog, DWLP_USER));
            if (!state || state->modelUpdateRunning || state->testRunning) return TRUE;
            state->modelUpdateRunning = true;
            const bool includePaid = IsDlgButtonChecked(dialog, IDC_SETTINGS_INCLUDE_PAID) == BST_CHECKED;
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, L"Googleのモデル一覧と公式料金ページを照合しています...");
            EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_UPDATE_MODELS), FALSE);
            EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_TEST), FALSE);
            EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), FALSE);
            EnableWindow(GetDlgItem(dialog, IDCANCEL), FALSE);
            auto* args = new ModelUpdateArgs{ dialog, includePaid };
            HANDLE thread = CreateThread(nullptr, 0, ModelUpdateThread, args, 0, nullptr);
            if (!thread)
            {
                delete args;
                state->modelUpdateRunning = false;
                EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_UPDATE_MODELS), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_TEST), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), TRUE);
                EnableWindow(GetDlgItem(dialog, IDCANCEL), TRUE);
                SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, L"モデル一覧の更新を開始できませんでした。");
            }
            else CloseHandle(thread);
            return TRUE;
        }

        case IDC_SETTINGS_TEST:
        {
            auto* state = reinterpret_cast<SettingsState*>(
                GetWindowLongPtrW(dialog, DWLP_USER));
            if (state == nullptr || state->testRunning || state->modelUpdateRunning) return TRUE;

            state->testRunning = true;
            ShowApiProcessingStatus(dialog, IDC_SETTINGS_STATUS,
                                    L"Gemini APIへ接続しています...");
            EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_TEST), FALSE);
            EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_UPDATE_MODELS), FALSE);
            EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), FALSE);
            EnableWindow(GetDlgItem(dialog, IDCANCEL), FALSE);

            HANDLE thread = CreateThread(nullptr, 0, GeminiTestThread, dialog, 0, nullptr);
            if (thread == nullptr)
            {
                state->testRunning = false;
                EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_TEST), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_UPDATE_MODELS), TRUE);
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
            if (state != nullptr && (state->testRunning || state->modelUpdateRunning)) return TRUE;
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        }
        }
        break;

    case WM_CLOSE:
    {
        auto* state = reinterpret_cast<SettingsState*>(
            GetWindowLongPtrW(dialog, DWLP_USER));
        if (state != nullptr && (state->testRunning || state->modelUpdateRunning)) return TRUE;
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
