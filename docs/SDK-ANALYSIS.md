# Becky! Plugin SDK analysis

Source archive: `B2PinSDK-26400` supplied for this project.

## Confirmed SDK declarations

- Menu constants are defined in `BeckyApi.h`.
- `BKC_MENU_LISTVIEW` is the ListView context menu.
- `BKC_MENU_MSGVIEW` is the message-view context menu.
- `BKC_OnMenuInit(HWND, HMENU, int)` is the menu initialization callback.
- `CBeckyAPI::RegisterCommand(LPCSTR, int, void (CALLBACK*)(HWND, LPARAM))` registers a command callback.
- The registered callback receives the menu command ID in the low word of LPARAM.
- `CBeckyAPI::GetNextMail(int, LPSTR, int, BOOL)` enumerates list items; with `bSelected=TRUE`, it enumerates selected items.
- `CBeckyAPI::GetCurrentMail()` returns the current mail ID.
- `CBeckyAPI::GetSource(LPCTSTR)` returns the raw, undecoded message source. The returned pointer becomes invalid after the callback exits or when released with `CBeckyAPI::Free()`.

## Current implementation scope

STEP 1 uses only the menu-registration declarations and callbacks. Mail retrieval, MIME parsing, reply composition, and Gemini requests are intentionally not implemented yet.

## Build compatibility

The supplied sample project is a Win32 DLL project. The SDK header uses TCHAR-based declarations, while its sample and documentation use ANSI strings in the Becky! API boundary. The initial project therefore compiles the Becky! SDK boundary as MBCS and uses Unicode Win32 APIs for the new UI. The exact build and runtime behavior still requires validation on the user's Becky! installation.
