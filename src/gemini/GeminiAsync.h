#pragma once

#include <windows.h>
#include <functional>
#include <new>
#include <string>
#include <utility>
#include "GeminiGenerate.h"

namespace GeminiAsync
{
    constexpr UINT WM_GENERATION_COMPLETE = WM_APP + 0x351;

    // Ownership of this object transfers from the caller to the worker thread,
    // then to the dialog thread through WM_GENERATION_COMPLETE.
    struct WorkItem
    {
        HWND target = nullptr;
        WPARAM tag = 0;
        std::wstring apiKey;
        std::function<GeminiGenerateResult(const std::wstring&)> operation;
        GeminiGenerateResult result{};
    };

    inline DWORD WINAPI ThreadProc(LPVOID parameter)
    {
        WorkItem* work = static_cast<WorkItem*>(parameter);
        try
        {
            work->result = work->operation(work->apiKey);
        }
        catch (...)
        {
            work->result.success = false;
            work->result.httpStatus = 0;
            work->result.text.clear();
            work->result.error = L"バックグラウンド処理中に予期しないエラーが発生しました。";
        }

        if (!work->apiKey.empty())
            SecureZeroMemory(work->apiKey.data(), work->apiKey.size() * sizeof(wchar_t));
        work->apiKey.clear();

        // The dialog remains open while busy. If posting nevertheless fails,
        // release the payload here rather than leaking it.
        if (!PostMessageW(work->target, WM_GENERATION_COMPLETE, work->tag,
                          reinterpret_cast<LPARAM>(work)))
            delete work;
        return 0;
    }

    inline bool Start(
        HWND target,
        std::wstring apiKey,
        std::function<GeminiGenerateResult(const std::wstring&)> operation,
        WPARAM tag = 0)
    {
        if (!IsWindow(target) || !operation) return false;

        WorkItem* work = new (std::nothrow) WorkItem;
        if (!work)
        {
            if (!apiKey.empty()) SecureZeroMemory(apiKey.data(), apiKey.size() * sizeof(wchar_t));
            return false;
        }

        try
        {
            work->target = target;
            work->tag = tag;
            work->apiKey = std::move(apiKey);
            work->operation = std::move(operation);
        }
        catch (...)
        {
            if (!work->apiKey.empty())
                SecureZeroMemory(work->apiKey.data(), work->apiKey.size() * sizeof(wchar_t));
            delete work;
            return false;
        }

        HANDLE thread = CreateThread(nullptr, 0, ThreadProc, work, 0, nullptr);
        if (!thread)
        {
            if (!work->apiKey.empty())
                SecureZeroMemory(work->apiKey.data(), work->apiKey.size() * sizeof(wchar_t));
            delete work;
            return false;
        }
        CloseHandle(thread);
        return true;
    }
}
