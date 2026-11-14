#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include "ui/ResultDialog.h"
#include "mail/MimeTextExtractor.h"
#include "BeckyAPI.h"
#include "ui/LauncherDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/ComposeDialog.h"
#include <unordered_map>

bool OpenReplyComposer(const std::wstring& mailId, const std::wstring& replyText, bool replyAll);

namespace
{
    HINSTANCE g_instance = nullptr;
    CBeckyAPI g_beckyApi;
    UINT g_launcherCommandList = 0;
    UINT g_launcherCommandView = 0;
    UINT g_newMailCommand = 0;
    std::unordered_map<HWND, int> g_composeModes;
    std::wstring g_pendingReplyText;
    ULONGLONG g_pendingReplyTick = 0;
    int g_pendingComposeMode = COMPOSE_MODE_REPLY1;

    std::string TrimAscii(const std::string& value)
    {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            return {};
        }
        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    bool EqualsAsciiNoCase(const std::string& left, const char* right)
    {
        const size_t rightLength = strlen(right);
        if (left.size() != rightLength)
        {
            return false;
        }

        for (size_t i = 0; i < rightLength; ++i)
        {
            if (std::tolower(static_cast<unsigned char>(left[i])) !=
                std::tolower(static_cast<unsigned char>(right[i])))
            {
                return false;
            }
        }
        return true;
    }

    std::wstring ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int required = MultiByteToWideChar(
            CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (required <= 0)
        {
            return L"(文字コードを変換できません)";
        }

        std::wstring result(static_cast<size_t>(required), L'\0');
        MultiByteToWideChar(
            CP_ACP, 0, value.data(), static_cast<int>(value.size()),
            result.data(), required);
        return result;
    }

    void ParseMailHeader(const char* header, MailContext& mail)
    {
        if (header == nullptr)
        {
            return;
        }

        // Header data is supplied by an external message. Bound the copy so
        // malformed messages cannot cause an unbounded scan or allocation.
        constexpr size_t kMaxHeaderBytes = 1024 * 1024;
        const size_t length = strnlen_s(header, kMaxHeaderBytes);
        const std::string headers(header, length);

        std::string currentField;
        std::string currentValue;

        const auto saveField = [&]()
        {
            if (EqualsAsciiNoCase(currentField, "Subject") && mail.subject.empty())
            {
                mail.subject = ToWide(TrimAscii(currentValue));
            }
            else if (EqualsAsciiNoCase(currentField, "From") && mail.from.empty())
            {
                mail.from = ToWide(TrimAscii(currentValue));
            }
        };

        size_t position = 0;
        while (position <= headers.size())
        {
            const size_t end = headers.find('\n', position);
            const size_t count = (end == std::string::npos)
                ? headers.size() - position
                : end - position;
            std::string line = headers.substr(position, count);
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            if (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            {
                if (!currentField.empty())
                {
                    currentValue += " " + TrimAscii(line);
                }
            }
            else
            {
                saveField();
                currentField.clear();
                currentValue.clear();

                const size_t colon = line.find(':');
                if (colon != std::string::npos)
                {
                    currentField = TrimAscii(line.substr(0, colon));
                    currentValue = TrimAscii(line.substr(colon + 1));
                }
            }

            if (end == std::string::npos)
            {
                break;
            }
            position = end + 1;
        }
        saveField();
    }

    void ShowMailUnavailable(HWND owner, const wchar_t* message)
    {
        MessageBoxW(owner, message, L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
    }

    void LoadMailBody(const std::string& mailId, MailContext& mail)
    {
        constexpr size_t kMaxSourceBytes = 5 * 1024 * 1024;
        char* source = g_beckyApi.GetSource(mailId.c_str());
        if (source == nullptr)
        {
            mail.bodyStatus = L"メール本文を取得できませんでした。";
            return;
        }

        const size_t sourceLength = strnlen_s(source, kMaxSourceBytes + 1);
        if (sourceLength > kMaxSourceBytes)
        {
            g_beckyApi.Free(source);
            mail.bodyStatus = L"メールサイズが上限（5 MiB）を超えています。";
            return;
        }

        std::string sourceCopy(source, sourceLength);
        g_beckyApi.Free(source);

        std::wstring error;
        if (!ExtractMailText(sourceCopy.data(), sourceCopy.size(), mail.body, error))
        {
            mail.bodyStatus = error.empty() ? L"本文を抽出できませんでした。" : error;
            return;
        }
        mail.bodyStatus = L"本文を取得しました（添付ファイルは対象外）。";
    }

    bool GetSingleSelectedMail(HWND owner, MailContext& mail)
    {
        char firstId[4096] = {};
        int position = g_beckyApi.GetNextMail(-1, firstId,
            static_cast<int>(sizeof(firstId)), TRUE);
        firstId[sizeof(firstId) - 1] = '\0';

        if (position == -1 || firstId[0] == '\0')
        {
            ShowMailUnavailable(owner, L"メール一覧で対象メールを1通選択してください。");
            return false;
        }

        char secondId[4096] = {};
        const int secondPosition = g_beckyApi.GetNextMail(position, secondId,
            static_cast<int>(sizeof(secondId)), TRUE);
        if (secondPosition != -1)
        {
            ShowMailUnavailable(owner, L"複数のメールが選択されています。1通だけ選択してください。");
            return false;
        }

        mail.mailId = ToWide(firstId);
        ParseMailHeader(g_beckyApi.GetHeader(firstId), mail);
        LoadMailBody(firstId, mail);
        return true;
    }

    bool GetCurrentDisplayedMail(HWND owner, MailContext& mail)
    {
        const char* currentId = g_beckyApi.GetCurrentMail();
        if (currentId == nullptr || currentId[0] == '\0')
        {
            ShowMailUnavailable(owner, L"表示中のメールを取得できませんでした。");
            return false;
        }

        // Copy the SDK-owned string before making another Becky! API call.
        const std::string mailId(currentId);
        mail.mailId = ToWide(mailId);
        ParseMailHeader(g_beckyApi.GetHeader(mailId.c_str()), mail);
        LoadMailBody(mailId, mail);
        return true;
    }

    void WINAPI OpenAssistantFromList(HWND owner, LPARAM)
    {
        MailContext mail;
        if (GetSingleSelectedMail(owner, mail))
        {
            ShowLauncherDialog(owner, g_instance, mail);
            if (mail.insertReplyRequested && !OpenReplyComposer(mail.mailId, mail.replyDraft, mail.replyAllRequested))
                MessageBoxW(owner, L"Becky!の返信画面を開けませんでした。対象メールを選択し直して、もう一度お試しください。",
                    L"Becky AI Assistant", MB_OK | MB_ICONWARNING);
        }
    }

    void WINAPI OpenAssistantFromView(HWND owner, LPARAM)
    {
        MailContext mail;
        if (GetCurrentDisplayedMail(owner, mail))
        {
            ShowLauncherDialog(owner, g_instance, mail);
            if (mail.insertReplyRequested)
                OpenReplyComposer(mail.mailId, mail.replyDraft, mail.replyAllRequested);
        }
    }

    void WINAPI OpenAssistantSettings(HWND owner, LPARAM)
    {
        ShowSettingsDialog(owner, g_instance);
    }

    UINT FindMenuCommandByText(HMENU menu, const wchar_t* target)
    {
        if (!menu || !target || !*target) return 0;

        const int count = GetMenuItemCount(menu);
        for (int i = 0; i < count; ++i) {
            wchar_t label[256]{};
            GetMenuStringW(menu, i, label, static_cast<int>(_countof(label)), MF_BYPOSITION);
            if (wcsstr(label, target) != nullptr) {
                const UINT commandId = GetMenuItemID(menu, i);
                if (commandId != static_cast<UINT>(-1) && commandId != 0)
                    return commandId;
            }

            HMENU submenu = GetSubMenu(menu, i);
            if (submenu) {
                const UINT commandId = FindMenuCommandByText(submenu, target);
                if (commandId != 0) return commandId;
            }
        }
        return 0;
    }

    void AddAssistantMenuItem(
        HMENU menu,
        int menuType,
        UINT& commandId,
        void (CALLBACK* callback)(HWND, LPARAM))
    {
        if (menu == nullptr)
        {
            return;
        }

        commandId = g_beckyApi.RegisterCommand(
            "Open Becky AI Assistant",
            menuType,
            callback);

        if (commandId != 0)
        {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(
                menu,
                MF_STRING,
                static_cast<UINT_PTR>(commandId),
                L"Becky AI Assistant...");
        }
    }
}

// The official SDK template initializes the Becky! API from DllMain.
// InitAPI only resolves BKA_* exports from the Becky! process module.
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);

        if (!g_beckyApi.InitAPI())
        {
            return FALSE;
        }
    }
    return TRUE;
}

// Required by the Becky! SDK. Becky! silently ignores a plug-in if this
// callback is missing or the name/vendor fields are left empty.
typedef struct tagBKPLUGININFO
{
    char szPlugInName[80];
    char szVendor[80];
    char szVersion[80];
    char szDescription[256];
} BKPLUGININFO, *LPBKPLUGININFO;

extern "C"
{
    int WINAPI BKC_OnPlugInInfo(LPBKPLUGININFO info)
    {
        if (info == nullptr)
        {
            return 0;
        }

        lstrcpynA(info->szPlugInName, "Becky AI Assistant",
                  static_cast<int>(sizeof(info->szPlugInName)));
        lstrcpynA(info->szVendor, "noname2007jp",
                  static_cast<int>(sizeof(info->szVendor)));
        lstrcpynA(info->szVersion, "0.18.0",
                  static_cast<int>(sizeof(info->szVersion)));
        lstrcpynA(info->szDescription,
                  "AI assistant launcher for Becky! Internet Mail",
                  static_cast<int>(sizeof(info->szDescription)));
        return 0;
    }

    int WINAPI BKC_OnPlugInSetup(HWND owner)
    {
        ShowSettingsDialog(owner, g_instance);
        return 1;
    }

    int WINAPI BKC_OnStart()
    {
        return 0;
    }

    int WINAPI BKC_OnExit()
    {
        return 0;
    }

    int WINAPI BKC_OnMenuInit(HWND, HMENU menu, int menuType)
    {
        switch (menuType)
        {
        case BKC_MENU_LISTVIEW:
            AddAssistantMenuItem(
                menu, menuType, g_launcherCommandList, OpenAssistantFromList);
            break;

        case BKC_MENU_MSGVIEW:
            AddAssistantMenuItem(
                menu, menuType, g_launcherCommandView, OpenAssistantFromView);
            break;

        default:
            break;
        }

        return 0;
    }
}

bool OpenReplyComposer(const std::wstring& mailId, const std::wstring& replyText, bool replyAll)
{
    if (mailId.empty() || replyText.empty()) return false;

    const int idBytes = WideCharToMultiByte(CP_ACP, 0, mailId.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (idBytes <= 1) return false;
    std::string id(static_cast<size_t>(idBytes), '\0');
    if (!WideCharToMultiByte(CP_ACP, 0, mailId.c_str(), -1, id.data(), idBytes, nullptr, nullptr))
        return false;

    HWND mainWindow = nullptr;
    HWND tree = nullptr, list = nullptr, view = nullptr;
    if (!g_beckyApi.GetWindowHandles(&mainWindow, &tree, &list, &view) || !mainWindow)
        return false;

    // Keep the target message selected before invoking Becky!'s native reply command.
    g_beckyApi.SetCurrentMail(id.c_str());
    g_pendingReplyText = replyText;
    g_pendingReplyTick = GetTickCount64();
    g_pendingComposeMode = replyAll ? COMPOSE_MODE_REPLY2 : COMPOSE_MODE_REPLY1;

    SetForegroundWindow(mainWindow);
    SetActiveWindow(mainWindow);
    SetFocus(mainWindow);

    if (replyAll) {
        // Ctrl+Shift+R is NOT Becky!'s Reply All command (Shift invokes template selection).
        // Resolve the native menu command by its visible Japanese label instead.
        const UINT commandId = FindMenuCommandByText(GetMenu(mainWindow), L"全員に返信");
        if (commandId == 0) {
            g_pendingReplyText.clear();
            g_pendingReplyTick = 0;
            return false;
        }
        SendMessageW(mainWindow, WM_COMMAND, MAKEWPARAM(commandId, 0), 0);
    } else {
        INPUT input[4]{};
        input[0].type = INPUT_KEYBOARD; input[0].ki.wVk = VK_CONTROL;
        input[1].type = INPUT_KEYBOARD; input[1].ki.wVk = 'R';
        input[2].type = INPUT_KEYBOARD; input[2].ki.wVk = 'R'; input[2].ki.dwFlags = KEYEVENTF_KEYUP;
        input[3].type = INPUT_KEYBOARD; input[3].ki.wVk = VK_CONTROL; input[3].ki.dwFlags = KEYEVENTF_KEYUP;
        const UINT sent = SendInput(4, input, sizeof(INPUT));
        if (sent != 4) {
            g_pendingReplyText.clear();
            g_pendingReplyTick = 0;
            return false;
        }
    }
    return true;
}

extern "C" int WINAPI BKC_OnOpenCompose(HWND composeWindow, int mode)
{
    if (g_pendingReplyText.empty())
        return 0;

    // Never let a draft intended for one compose mode leak into a later manual action.
    if (composeWindow == nullptr || GetTickCount64() - g_pendingReplyTick > 15000 ||
        mode != g_pendingComposeMode) {
        g_pendingReplyText.clear();
        g_pendingReplyTick = 0;
        return 0;
    }

    std::wstring insertion = g_pendingReplyText;
    if (insertion.size() < 2 || insertion.substr(insertion.size() - 2) != L"\r\n")
        insertion += L"\r\n\r\n";
    const int bytes = WideCharToMultiByte(CP_ACP, 0, insertion.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
        g_pendingReplyText.clear();
        g_pendingReplyTick = 0;
        return 0;
    }
    std::string text(static_cast<size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_ACP, 0, insertion.c_str(), -1, text.data(), bytes, nullptr, nullptr)) {
        // nMode=-1 inserts the draft at the beginning and preserves Becky!'s quoted original message.
        g_beckyApi.CompSetText(composeWindow, -1, text.c_str());
    }
    g_pendingReplyText.clear();
    g_pendingReplyTick = 0;
    return 0;
}
