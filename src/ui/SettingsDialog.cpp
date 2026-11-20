#include "SettingsDialog.h"
#include "resource.h"
#include "ProgressIndicator.h"
#include "settings/ApiKeyStore.h"
#include "settings/AIProviderSettings.h"
#include "gemini/GeminiConnection.h"
#include "gemini/GeminiModels.h"
#include "providers/ProviderModelCatalog.h"
#include "providers/ProviderModelCatalogStore.h"
#include "providers/ModelDescriptions.h"
#include "providers/ModelCatalogPolicy.h"

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
constexpr UINT WM_PROVIDER_MODELS_FINISHED = WM_APP + 43;

struct SettingsState
{
    bool testRunning = false;
    bool apiKeyVisible = false;
    bool modelUpdateRunning = false;
    bool providerModelUpdateRunning = false;
    std::vector<GeminiModelOption> models;
};


void ShowSettingsTab(HWND dialog, int selected)
{
    const int basic[] = { IDC_SETTINGS_LABEL_API, IDC_GEMINI_API_KEY, IDC_SETTINGS_TOGGLE_KEY,
        IDC_GEMINI_APIKEY_LINK, IDC_SETTINGS_LABEL_API_NOTE, IDC_SETTINGS_LABEL_MODEL,
        IDC_GROQ_APIKEY_LINK, IDC_CF_WORKERS_AI_LINK, IDC_CF_AI_GATEWAY_LINK,
        IDC_SETTINGS_DEFAULT_MODEL, IDC_SETTINGS_PRICING_LINK, IDC_SETTINGS_UPDATE_MODELS,
        IDC_SETTINGS_TEST, IDC_GEMINI_MODEL_DETAILS };
    const int provider[] = { IDC_PROVIDER_LABEL, IDC_PROVIDER_SELECT, IDC_PROVIDER_ROUTE_LABEL,
        IDC_PROVIDER_ROUTE, IDC_PROVIDER_STATUS_NOTE, IDC_PROVIDER_GROQ_KEY_LABEL,
        IDC_PROVIDER_GROQ_KEY, IDC_PROVIDER_GROQ_MODEL_LABEL, IDC_PROVIDER_GROQ_MODEL,
        IDC_PROVIDER_WORKERS_ACCOUNT_LABEL, IDC_PROVIDER_WORKERS_ACCOUNT,
        IDC_PROVIDER_WORKERS_TOKEN_LABEL, IDC_PROVIDER_WORKERS_TOKEN,
        IDC_PROVIDER_WORKERS_MODEL_LABEL, IDC_PROVIDER_WORKERS_MODEL,
        IDC_PROVIDER_GROQ_MODELS_UPDATE, IDC_PROVIDER_WORKERS_MODELS_UPDATE,
        IDC_PROVIDER_MODEL_DETAILS };
    const int gateway[] = { IDC_GATEWAY_ACCOUNT_LABEL, IDC_GATEWAY_ACCOUNT, IDC_GATEWAY_ID_LABEL,
        IDC_GATEWAY_ID, IDC_GATEWAY_TOKEN_LABEL, IDC_GATEWAY_TOKEN, IDC_GATEWAY_LOGGING,
        IDC_GATEWAY_CACHE, IDC_GATEWAY_PRIVACY_NOTE };
    const int masking[] = { IDC_SETTINGS_LABEL_MASK, IDC_MASK_EMAIL, IDC_MASK_PHONE,
        IDC_MASK_POSTAL, IDC_MASK_IP, IDC_MASK_COMPANY, IDC_MASK_PERSON, IDC_MASK_ADDRESS,
        IDC_MASK_ACCOUNT, IDC_MASK_CUSTOM, IDC_SETTINGS_LABEL_MASK_TERMS,
        IDC_SETTINGS_MASK_TERMS, IDC_SETTINGS_LABEL_MASK_NOTE };
    const int advanced[] = { IDC_SETTINGS_AUTO_FALLBACK, IDC_SETTINGS_INCLUDE_PAID,
        IDC_SETTINGS_DEBUG_LOG, IDC_SETTINGS_LABEL_DEBUG_NOTE };
    for (int id : basic) ShowWindow(GetDlgItem(dialog, id), selected == 0 ? SW_SHOW : SW_HIDE);
    for (int id : masking) ShowWindow(GetDlgItem(dialog, id), selected == 1 ? SW_SHOW : SW_HIDE);
    for (int id : advanced) ShowWindow(GetDlgItem(dialog, id), selected == 2 ? SW_SHOW : SW_HIDE);
    for (int id : provider) ShowWindow(GetDlgItem(dialog, id), selected == 3 ? SW_SHOW : SW_HIDE);
    for (int id : gateway) ShowWindow(GetDlgItem(dialog, id), selected == 4 ? SW_SHOW : SW_HIDE);
}

void UpdateGeminiModelDescription(HWND dialog, SettingsState* state)
{
    if (!state) return;
    const LRESULT selected = SendDlgItemMessageW(dialog, IDC_SETTINGS_DEFAULT_MODEL, CB_GETCURSEL, 0, 0);
    if (selected >= 0 && selected < static_cast<LRESULT>(state->models.size()))
        {
            const auto& modelId = state->models[static_cast<size_t>(selected)].id;
            SetDlgItemTextW(dialog, IDC_GEMINI_MODEL_DETAILS,
                (GetModelDescription(0, modelId) + L"\\r\\n" + GetModelCatalogPolicyNote(0, modelId)).c_str());
        }
    else
        SetDlgItemTextW(dialog, IDC_GEMINI_MODEL_DETAILS, L"モデルを選択すると特徴を表示します。");
}

void UpdateProviderModelDescription(HWND dialog, int provider)
{
    const int control = provider == 1 ? IDC_PROVIDER_GROQ_MODEL : IDC_PROVIDER_WORKERS_MODEL;
    const int length = GetWindowTextLengthW(GetDlgItem(dialog, control));
    std::wstring model(static_cast<size_t>(length) + 1, L'\0');
    if (length > 0) GetDlgItemTextW(dialog, control, model.data(), length + 1);
    model.resize(static_cast<size_t>(length));
    SetDlgItemTextW(dialog, IDC_PROVIDER_MODEL_DETAILS,
        (GetModelDescription(provider, model) + L"\\r\\n" + GetModelCatalogPolicyNote(provider, model)).c_str());
}

void UpdateCurrentProviderModelDescription(HWND dialog)
{
    const int provider = static_cast<int>(SendDlgItemMessageW(dialog, IDC_PROVIDER_SELECT, CB_GETCURSEL, 0, 0));
    if (provider == 1 || provider == 2) UpdateProviderModelDescription(dialog, provider);
    else SetDlgItemTextW(dialog, IDC_PROVIDER_MODEL_DETAILS,
        L"Geminiのモデル説明は「基本」タブに表示します。");
}

void PopulateProviderModelCombo(HWND dialog, int provider,
    const std::vector<std::wstring>& models, const std::wstring& preferred)
{
    const int control = provider == 1 ? IDC_PROVIDER_GROQ_MODEL : IDC_PROVIDER_WORKERS_MODEL;
    HWND combo = GetDlgItem(dialog, control);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    int selected = -1;
    for (size_t i = 0; i < models.size(); ++i)
    {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(models[i].c_str()));
        if (models[i] == preferred) selected = static_cast<int>(i);
    }
    if (selected >= 0) SendMessageW(combo, CB_SETCURSEL, selected, 0);
    else SetWindowTextW(combo, preferred.c_str());
}

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


struct ProviderModelsResult
{
    int provider = 0;
    std::vector<std::wstring> models;
    std::wstring message;
    bool success = false;
};
struct ProviderModelsArgs
{
    HWND dialog;
    int provider;
    std::wstring apiKey;
    std::wstring accountId;
    std::wstring apiToken;
};
DWORD WINAPI ProviderModelsThread(void* parameter)
{
    std::unique_ptr<ProviderModelsArgs> args(static_cast<ProviderModelsArgs*>(parameter));
    auto* result = new ProviderModelsResult{};
    result->provider = args->provider;
    if (args->provider == 1)
        result->success = FetchGroqModelCatalog(args->apiKey, result->models, result->message);
    else
        result->success = FetchWorkersAIModelCatalog(args->accountId, args->apiToken, result->models, result->message);
    if (result->success)
    {
        std::wstring policyError;
        if (!RefreshModelCatalogPolicy(policyError))
            result->message = L"モデル一覧は取得しました。評価情報は保存済みデータを使用します。";
    }
    for (auto* secret : { &args->apiKey, &args->apiToken })
        if (!secret->empty()) SecureZeroMemory(secret->data(), secret->size() * sizeof(wchar_t));
    if (result->success)
        result->message = L"モデル一覧を取得しました（" + std::to_wstring(result->models.size()) + L"件）。Free枠の対象かどうかはこのAPIでは判定できません。";
    if (!PostMessageW(args->dialog, WM_PROVIDER_MODELS_FINISHED, 0, reinterpret_cast<LPARAM>(result))) delete result;
    return 0;
}

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
            std::wstring policyError;
            if (!RefreshModelCatalogPolicy(policyError))
                result->message = L"モデル一覧は取得しました。評価情報は保存済みデータを使用します。";
        }
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
        HWND tabs = GetDlgItem(dialog, IDC_SETTINGS_TABS);
        TCITEMW tab{}; tab.mask = TCIF_TEXT;
        tab.pszText = const_cast<wchar_t*>(L"\u57FA\u672C"); SendMessageW(tabs, TCM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&tab));
        tab.pszText = const_cast<wchar_t*>(L"\u30DE\u30B9\u30AD\u30F3\u30B0"); SendMessageW(tabs, TCM_INSERTITEMW, 1, reinterpret_cast<LPARAM>(&tab));
        tab.pszText = const_cast<wchar_t*>(L"\u8A73\u7D30"); SendMessageW(tabs, TCM_INSERTITEMW, 2, reinterpret_cast<LPARAM>(&tab));
        tab.pszText = const_cast<wchar_t*>(L"AIサービス"); SendMessageW(tabs, TCM_INSERTITEMW, 3, reinterpret_cast<LPARAM>(&tab));
        tab.pszText = const_cast<wchar_t*>(L"AI Gateway"); SendMessageW(tabs, TCM_INSERTITEMW, 4, reinterpret_cast<LPARAM>(&tab));
        TabCtrl_SetCurSel(tabs, 0);
        ShowSettingsTab(dialog, 0);
        std::wstring key, error;
        std::wstring policyCacheError;
        LoadModelCatalogPolicy(policyCacheError);
        SetDlgItemTextW(dialog, IDC_GEMINI_APIKEY_LINK,
            L"<a href=\"https://aistudio.google.com/api-keys\">Google AI StudioでAPIキーを取得</a>");
        SetDlgItemTextW(dialog, IDC_SETTINGS_PRICING_LINK,
            L"<a href=\"https://aistudio.google.com/docs/pricing\">Google公式料金ページを開く</a>");
        SetDlgItemTextW(dialog, IDC_GROQ_APIKEY_LINK,
            L"<a href=\"https://console.groq.com/keys\">Groq APIキーを取得</a>");
        SetDlgItemTextW(dialog, IDC_CF_WORKERS_AI_LINK,
            L"<a href=\"https://developers.cloudflare.com/workers-ai/get-started/rest-api/\">Cloudflare Workers AI：API Token取得手順</a>");
        SetDlgItemTextW(dialog, IDC_CF_AI_GATEWAY_LINK,
            L"<a href=\"https://developers.cloudflare.com/ai-gateway/get-started/\">Cloudflare AI Gateway：セットアップ手順</a>");
        if (!LoadGeminiApiKey(key, error))
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, error.c_str());
        else if (!key.empty())
            SetDlgItemTextW(dialog, IDC_GEMINI_API_KEY, key.c_str());
        if (!key.empty()) SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
        AIProviderSettings providerSettings;
        std::wstring providerError;
        if (LoadAIProviderSettings(providerSettings, providerError))
        {
            HWND providerCombo = GetDlgItem(dialog, IDC_PROVIDER_SELECT);
            SendMessageW(providerCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Gemini"));
            SendMessageW(providerCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Groq"));
            SendMessageW(providerCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cloudflare Workers AI"));
            SendMessageW(providerCombo, CB_SETCURSEL, static_cast<WPARAM>(providerSettings.provider), 0);
            HWND routeCombo = GetDlgItem(dialog, IDC_PROVIDER_ROUTE);
            SendMessageW(routeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"直接接続"));
            SendMessageW(routeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cloudflare AI Gateway経由"));
            SendMessageW(routeCombo, CB_SETCURSEL, static_cast<WPARAM>(providerSettings.route), 0);
            SetDlgItemTextW(dialog, IDC_PROVIDER_GROQ_KEY, providerSettings.groqApiKey.c_str());
            SetDlgItemTextW(dialog, IDC_PROVIDER_GROQ_MODEL, providerSettings.groqModel.c_str());
            SetDlgItemTextW(dialog, IDC_PROVIDER_WORKERS_ACCOUNT, providerSettings.workersAccountId.c_str());
            SetDlgItemTextW(dialog, IDC_PROVIDER_WORKERS_TOKEN, providerSettings.workersApiToken.c_str());
            SetDlgItemTextW(dialog, IDC_PROVIDER_WORKERS_MODEL, providerSettings.workersModel.c_str());
            // Restore the last successfully fetched catalogs immediately; network refresh is optional.
            for (int provider : { 1, 2 })
            {
                std::vector<std::wstring> cachedModels;
                std::wstring cacheError;
                if (LoadProviderModelCatalog(provider, cachedModels, cacheError) && !cachedModels.empty())
                {
                    const int modelControl = provider == 1 ? IDC_PROVIDER_GROQ_MODEL : IDC_PROVIDER_WORKERS_MODEL;
                    const int modelLength = GetWindowTextLengthW(GetDlgItem(dialog, modelControl));
                    std::wstring preferred(static_cast<size_t>(modelLength) + 1, L'\0');
                    if (modelLength > 0) GetDlgItemTextW(dialog, modelControl, preferred.data(), modelLength + 1);
                    preferred.resize(static_cast<size_t>(modelLength));
                    PopulateProviderModelCombo(dialog, provider, cachedModels, preferred);
                }
            }
            SetDlgItemTextW(dialog, IDC_GATEWAY_ACCOUNT, providerSettings.cloudflareAccountId.c_str());
            SetDlgItemTextW(dialog, IDC_GATEWAY_ID, providerSettings.cloudflareGatewayId.c_str());
            SetDlgItemTextW(dialog, IDC_GATEWAY_TOKEN, providerSettings.cloudflareApiToken.c_str());
            CheckDlgButton(dialog, IDC_GATEWAY_LOGGING, providerSettings.gatewayRequestLogging ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dialog, IDC_GATEWAY_CACHE, providerSettings.gatewayCacheEnabled ? BST_CHECKED : BST_UNCHECKED);
            for (auto* secret : { &providerSettings.geminiApiKey, &providerSettings.groqApiKey,
                &providerSettings.workersApiToken, &providerSettings.cloudflareApiToken })
                if (!secret->empty()) SecureZeroMemory(secret->data(), secret->size() * sizeof(wchar_t));
        }
        else
        {
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, providerError.c_str());
            SendDlgItemMessageW(dialog, IDC_PROVIDER_SELECT, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Gemini"));
            SendDlgItemMessageW(dialog, IDC_PROVIDER_SELECT, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Groq"));
            SendDlgItemMessageW(dialog, IDC_PROVIDER_SELECT, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cloudflare Workers AI"));
            SendDlgItemMessageW(dialog, IDC_PROVIDER_SELECT, CB_SETCURSEL, 0, 0);
            SendDlgItemMessageW(dialog, IDC_PROVIDER_ROUTE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"直接接続"));
            SendDlgItemMessageW(dialog, IDC_PROVIDER_ROUTE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cloudflare AI Gateway経由"));
            SendDlgItemMessageW(dialog, IDC_PROVIDER_ROUTE, CB_SETCURSEL, 0, 0);
        }
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
        UpdateGeminiModelDescription(dialog, state);
        UpdateCurrentProviderModelDescription(dialog);
        return TRUE;
    }

    case WM_CTLCOLORDLG:
        return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));
    case WM_CTLCOLORSTATIC:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_NOTIFY:
    {
        const auto* tabNotification = reinterpret_cast<const NMHDR*>(lParam);
        if (tabNotification && tabNotification->idFrom == IDC_SETTINGS_TABS && tabNotification->code == TCN_SELCHANGE) {
            const int selectedTab = TabCtrl_GetCurSel(GetDlgItem(dialog, IDC_SETTINGS_TABS));
            ShowSettingsTab(dialog, selectedTab);
            if (selectedTab == 3) UpdateCurrentProviderModelDescription(dialog);
            return TRUE;
        }
        const auto* notification = reinterpret_cast<const NMHDR*>(lParam);
        if (notification != nullptr &&
            notification->idFrom == IDC_SETTINGS_PRICING_LINK &&
            (notification->code == NM_CLICK || notification->code == NM_RETURN))
        {
            ShellExecuteW(dialog, L"open", L"https://aistudio.google.com/docs/pricing", nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        if (notification != nullptr &&
            (notification->idFrom == IDC_GEMINI_APIKEY_LINK ||
             notification->idFrom == IDC_GROQ_APIKEY_LINK ||
             notification->idFrom == IDC_CF_WORKERS_AI_LINK ||
             notification->idFrom == IDC_CF_AI_GATEWAY_LINK) &&
            (notification->code == NM_CLICK || notification->code == NM_RETURN))
        {
            const wchar_t* url = L"https://aistudio.google.com/api-keys";
            if (notification->idFrom == IDC_GROQ_APIKEY_LINK)
                url = L"https://console.groq.com/keys";
            else if (notification->idFrom == IDC_CF_WORKERS_AI_LINK)
                url = L"https://developers.cloudflare.com/workers-ai/get-started/rest-api/";
            else if (notification->idFrom == IDC_CF_AI_GATEWAY_LINK)
                url = L"https://developers.cloudflare.com/ai-gateway/get-started/";
            ShellExecuteW(dialog, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
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
                UpdateGeminiModelDescription(dialog, state);
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

    case WM_PROVIDER_MODELS_FINISHED:
    {
        auto* state = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(dialog, DWLP_USER));
        auto* result = reinterpret_cast<ProviderModelsResult*>(lParam);
        if (result)
        {
            if (result->success)
            {
                const int control = result->provider == 1 ? IDC_PROVIDER_GROQ_MODEL : IDC_PROVIDER_WORKERS_MODEL;
                HWND combo = GetDlgItem(dialog, control);
                const int len = GetWindowTextLengthW(combo);
                std::wstring preferred(static_cast<size_t>(len) + 1, L'\0');
                if (len > 0) GetWindowTextW(combo, preferred.data(), len + 1);
                preferred.resize(static_cast<size_t>(len));
                PopulateProviderModelCombo(dialog, result->provider, result->models, preferred);
                UpdateProviderModelDescription(dialog, result->provider);
                std::wstring saveError;
                if (SaveProviderModelCatalog(result->provider, result->models, saveError))
                    result->message += L" 次回起動時もこの一覧を利用できます。";
                else
                    result->message += L" ただし、一覧の保存に失敗しました：" + saveError;
            }
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, result->message.c_str());
            delete result;
        }
        if (state) state->providerModelUpdateRunning = false;
        EnableWindow(GetDlgItem(dialog, IDC_PROVIDER_GROQ_MODELS_UPDATE), TRUE);
        EnableWindow(GetDlgItem(dialog, IDC_PROVIDER_WORKERS_MODELS_UPDATE), TRUE);
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
        if (LOWORD(wParam) == IDC_SETTINGS_DEFAULT_MODEL && HIWORD(wParam) == CBN_SELCHANGE)
            UpdateGeminiModelDescription(dialog, reinterpret_cast<SettingsState*>(GetWindowLongPtrW(dialog, DWLP_USER)));
        if (LOWORD(wParam) == IDC_PROVIDER_SELECT && HIWORD(wParam) == CBN_SELCHANGE)
            UpdateCurrentProviderModelDescription(dialog);
        if ((LOWORD(wParam) == IDC_PROVIDER_GROQ_MODEL || LOWORD(wParam) == IDC_PROVIDER_WORKERS_MODEL) &&
            (HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == EN_CHANGE))
            UpdateProviderModelDescription(dialog, LOWORD(wParam) == IDC_PROVIDER_GROQ_MODEL ? 1 : 2);
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
            AIProviderSettings providerSettings;
            providerSettings.provider = static_cast<AIProvider>(SendDlgItemMessageW(dialog, IDC_PROVIDER_SELECT, CB_GETCURSEL, 0, 0));
            providerSettings.route = static_cast<AIConnectionRoute>(SendDlgItemMessageW(dialog, IDC_PROVIDER_ROUTE, CB_GETCURSEL, 0, 0));
            auto readSetting = [dialog](int id) {
                const int length = GetWindowTextLengthW(GetDlgItem(dialog, id));
                std::wstring value(static_cast<size_t>(length) + 1, L'\0');
                if (length > 0) GetDlgItemTextW(dialog, id, value.data(), length + 1);
                value.resize(static_cast<size_t>(length));
                return value;
            };
            providerSettings.geminiApiKey = readSetting(IDC_GEMINI_API_KEY);
            providerSettings.groqApiKey = readSetting(IDC_PROVIDER_GROQ_KEY);
            providerSettings.groqModel = readSetting(IDC_PROVIDER_GROQ_MODEL);
            providerSettings.workersAccountId = readSetting(IDC_PROVIDER_WORKERS_ACCOUNT);
            providerSettings.workersApiToken = readSetting(IDC_PROVIDER_WORKERS_TOKEN);
            providerSettings.workersModel = readSetting(IDC_PROVIDER_WORKERS_MODEL);
            providerSettings.cloudflareAccountId = readSetting(IDC_GATEWAY_ACCOUNT);
            providerSettings.cloudflareGatewayId = readSetting(IDC_GATEWAY_ID);
            providerSettings.cloudflareApiToken = readSetting(IDC_GATEWAY_TOKEN);
            providerSettings.gatewayRequestLogging = IsDlgButtonChecked(dialog, IDC_GATEWAY_LOGGING) == BST_CHECKED;
            providerSettings.gatewayCacheEnabled = IsDlgButtonChecked(dialog, IDC_GATEWAY_CACHE) == BST_CHECKED;
            const bool providerSettingsSaved = SaveAIProviderSettings(providerSettings, error);
            for (auto* secret : { &providerSettings.geminiApiKey, &providerSettings.groqApiKey,
                &providerSettings.workersApiToken, &providerSettings.cloudflareApiToken })
                if (!secret->empty()) SecureZeroMemory(secret->data(), secret->size() * sizeof(wchar_t));

            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS,
                keySaved && termsSaved && optionsSaved && reliabilitySaved && providerSettingsSaved
                    ? L"設定を暗号化して保存しました。なお、メール処理の接続先は現時点ではGemini固定です。"
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

        case IDC_PROVIDER_GROQ_MODELS_UPDATE:
        case IDC_PROVIDER_WORKERS_MODELS_UPDATE:
        {
            auto* state = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(dialog, DWLP_USER));
            if (!state || state->providerModelUpdateRunning || state->modelUpdateRunning || state->testRunning) return TRUE;
            const int provider = LOWORD(wParam) == IDC_PROVIDER_GROQ_MODELS_UPDATE ? 1 : 2;
            auto read = [&](int id) {
                int len = GetWindowTextLengthW(GetDlgItem(dialog, id));
                std::wstring value(static_cast<size_t>(len) + 1, L'\0');
                if (len > 0) GetDlgItemTextW(dialog, id, value.data(), len + 1);
                value.resize(static_cast<size_t>(len));
                return value;
            };
            auto* args = new ProviderModelsArgs{ dialog, provider,
                provider == 1 ? read(IDC_PROVIDER_GROQ_KEY) : L"",
                provider == 2 ? read(IDC_PROVIDER_WORKERS_ACCOUNT) : L"",
                provider == 1 ? L"" : read(IDC_PROVIDER_WORKERS_TOKEN) };
            state->providerModelUpdateRunning = true;
            SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS,
                provider == 1 ? L"Groqの利用可能モデルを取得しています..." : L"Cloudflare Workers AIのテキスト生成モデルを取得しています...");
            EnableWindow(GetDlgItem(dialog, IDC_PROVIDER_GROQ_MODELS_UPDATE), FALSE);
            EnableWindow(GetDlgItem(dialog, IDC_PROVIDER_WORKERS_MODELS_UPDATE), FALSE);
            EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), FALSE);
            EnableWindow(GetDlgItem(dialog, IDCANCEL), FALSE);
            HANDLE thread = CreateThread(nullptr, 0, ProviderModelsThread, args, 0, nullptr);
            if (!thread)
            {
                delete args;
                state->providerModelUpdateRunning = false;
                EnableWindow(GetDlgItem(dialog, IDC_PROVIDER_GROQ_MODELS_UPDATE), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_PROVIDER_WORKERS_MODELS_UPDATE), TRUE);
                EnableWindow(GetDlgItem(dialog, IDC_SETTINGS_SAVE), TRUE);
                EnableWindow(GetDlgItem(dialog, IDCANCEL), TRUE);
                SetDlgItemTextW(dialog, IDC_SETTINGS_STATUS, L"モデル一覧の取得を開始できませんでした。");
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
            if (state != nullptr && (state->testRunning || state->modelUpdateRunning || state->providerModelUpdateRunning)) return TRUE;
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        }
        }
        break;

    case WM_CLOSE:
    {
        auto* state = reinterpret_cast<SettingsState*>(
            GetWindowLongPtrW(dialog, DWLP_USER));
        if (state != nullptr && (state->testRunning || state->modelUpdateRunning || state->providerModelUpdateRunning)) return TRUE;
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
    controls.dwICC = ICC_LINK_CLASS | ICC_TAB_CLASSES;
    InitCommonControlsEx(&controls);

    SettingsState state;
    return DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_AI_SETTINGS),
                           owner, SettingsProc, reinterpret_cast<LPARAM>(&state));
}
