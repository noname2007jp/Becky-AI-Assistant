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

extern "C"
{
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
