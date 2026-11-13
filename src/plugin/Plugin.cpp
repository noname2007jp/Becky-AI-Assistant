#include <windows.h>
#include "BeckyAPI.h"
#include "ui/LauncherDialog.h"

namespace
{
    HINSTANCE g_instance = nullptr;
    CBeckyAPI g_beckyApi;
    UINT g_launcherCommandList = 0;
    UINT g_launcherCommandView = 0;

    void WINAPI OpenAssistantCommand(HWND owner, LPARAM)
    {
        ShowLauncherDialog(owner, g_instance);
    }

    void AddAssistantMenuItem(HMENU menu, int menuType, UINT& commandId)
    {
        if (menu == nullptr)
        {
            return;
        }

        commandId = g_beckyApi.RegisterCommand(
            "Open Becky AI Assistant",
            menuType,
            OpenAssistantCommand);

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
        lstrcpynA(info->szVersion, "0.1.1",
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
            AddAssistantMenuItem(menu, menuType, g_launcherCommandList);
            break;

        case BKC_MENU_MSGVIEW:
            AddAssistantMenuItem(menu, menuType, g_launcherCommandView);
            break;

        default:
            break;
        }

        return 0;
    }
}
