#include <windows.h>
#include <algorithm>
#include <cctype>
#include <string>
#include "BeckyAPI.h"
#include "ui/LauncherDialog.h"

namespace
{
    HINSTANCE g_instance = nullptr;
    CBeckyAPI g_beckyApi;
    UINT g_launcherCommandList = 0;
    UINT g_launcherCommandView = 0;

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
        return true;
    }

    void WINAPI OpenAssistantFromList(HWND owner, LPARAM)
    {
        MailContext mail;
        if (GetSingleSelectedMail(owner, mail))
        {
            ShowLauncherDialog(owner, g_instance, mail);
        }
    }

    void WINAPI OpenAssistantFromView(HWND owner, LPARAM)
    {
        MailContext mail;
        if (GetCurrentDisplayedMail(owner, mail))
        {
            ShowLauncherDialog(owner, g_instance, mail);
        }
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
        lstrcpynA(info->szVersion, "0.2.0",
                  static_cast<int>(sizeof(info->szVersion)));
        lstrcpynA(info->szDescription,
                  "AI assistant launcher for Becky! Internet Mail",
                  static_cast<int>(sizeof(info->szDescription)));
        return 0;
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
