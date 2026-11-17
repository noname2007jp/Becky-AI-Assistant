#include "gemini/GeminiAsync.h"
#include <windows.h>
#include <atomic>
#include <iostream>

namespace
{
int failures = 0;

void Check(bool condition, const char* name)
{
    if (condition) std::cout << "[PASS] " << name << "\n";
    else { std::cout << "[FAIL] " << name << "\n"; ++failures; }
}

LRESULT CALLBACK TestWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(window, message, wParam, lParam);
}

bool WaitForCompletion(HWND window, GeminiAsync::WorkItem*& completed)
{
    MSG message{};
    if (GetMessageW(&message, window, GeminiAsync::WM_GENERATION_COMPLETE,
        GeminiAsync::WM_GENERATION_COMPLETE) <= 0)
        return false;
    completed = reinterpret_cast<GeminiAsync::WorkItem*>(message.lParam);
    return completed != nullptr;
}
}

int main()
{
    const wchar_t* className = L"BeckyAIAssistantAsyncTestWindow";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = TestWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = className;
    Check(RegisterClassW(&windowClass) != 0, "register test window class");

    HWND window = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, windowClass.hInstance, nullptr);
    Check(window != nullptr, "create message-only test window");
    if (!window) return 1;

    GeminiAsync::WorkItem* work = nullptr;
    const bool started = GeminiAsync::Start(window, L"test-key",
        [](const std::wstring&, const std::atomic<int>&) {
            GeminiGenerateResult result{ true, 200, L"ok", L"" };
            return result;
        }, 7, &work);
    Check(started && work != nullptr && work->threadHandle != nullptr,
        "worker handle and ownership pointer are initialized before execution");
    GeminiAsync::WorkItem* completed = nullptr;
    Check(WaitForCompletion(window, completed), "receive successful worker completion");
    if (completed) {
        Check(completed == work && completed->tag == 7 && completed->result.success,
            "completion returns the original work item and result");
        GeminiAsync::Destroy(completed);
    }

    work = nullptr;
    const bool cancelStarted = GeminiAsync::Start(window, L"test-key",
        [](const std::wstring&, const std::atomic<int>& state) {
            while (state.load(std::memory_order_acquire) == 0) Sleep(1);
            GeminiGenerateResult result{ true, 200, L"should be discarded", L"" };
            return result;
        }, 9, &work);
    Check(cancelStarted && work != nullptr, "start cancellable worker");
    if (cancelStarted && work) {
        GeminiAsync::RequestCancel(work);
        completed = nullptr;
        Check(WaitForCompletion(window, completed), "receive cancelled worker completion");
        if (completed) {
            Check(completed->result.cancelled && completed->result.text.empty(),
                "cancellation discards a late successful result");
            GeminiAsync::Destroy(completed);
        }
    }

    DestroyWindow(window);
    UnregisterClassW(className, windowClass.hInstance);
    return failures == 0 ? 0 : 1;
}
