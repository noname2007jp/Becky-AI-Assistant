#pragma once

#include <windows.h>
#include <functional>
#include <new>
#include "GeminiGenerate.h"

// A generation result is transferred to the dialog thread through a private
// window message. The dialog must remain alive until it receives completion.
namespace GeminiAsync
{
    constexpr UINT WM_GENERATION_COMPLETE = WM_APP + 0x351;

    struct WorkItem
    {
        HWND target = nullptr;
        std::function<GeminiGenerateResult()> operation;
    };

    inline DWORD WINAPI ThreadProc(LPVOID parameter)
    {
        WorkItem* work = static_cast<WorkItem*>(parameter);
        GeminiGenerateResult* result = new (std::nothrow) GeminiGenerateResult{};
        if (result)
        {
            try { *result = work->operation(); }
            catch (...)
            {
                result->success = false;
                result->error = L"バックグラウンド処理中に予期しないエラーが発生しました。";
            }
        }
        HWND target = work->target;
        delete work;
        if (!result) return 0;
        if (!PostMessageW(target, WM_GENERATION_COMPLETE, 0,
                          reinterpret_cast<LPARAM>(result)))
            delete result;
        return 0;
    }

    inline bool Start(HWND target, std::function<GeminiGenerateResult()> operation)
    {
        WorkItem* work = new (std::nothrow) WorkItem;
        if (!work) return false;
        work->target = target;
        work->operation = std::move(operation);
        HANDLE thread = CreateThread(nullptr, 0, ThreadProc, work, 0, nullptr);
        if (!thread) { delete work; return false; }
        CloseHandle(thread);
        return true;
    }
}
