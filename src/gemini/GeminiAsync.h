#pragma once

#include <windows.h>
#include <functional>
#include <atomic>
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
        HANDLE threadHandle = nullptr;
        std::atomic<int> state{ 0 }; // 0=running, 1=cancel requested, 2=finished
    };

    inline DWORD WINAPI ThreadProc(LPVOID parameter)
    {
        WorkItem* work = static_cast<WorkItem*>(parameter);
        try
        {
            if (work->state.load(std::memory_order_acquire) == 0)
                work->result = work->operation(work->apiKey);
        }
        catch (...)
        {
            work->result.success = false;
            work->result.httpStatus = 0;
            work->result.text.clear();
            work->result.error = L"バックグラウンド処理中に予期しないエラーが発生しました。";
        }

        int expectedState = 0;
        const bool completedNormally = work->state.compare_exchange_strong(
            expectedState, 2, std::memory_order_acq_rel);
        if (!completedNormally && expectedState == 1) {
            work->result.success = false;
            work->result.cancelled = true;
            work->result.text.clear();
            work->result.error.clear();
        }

        if (!work->apiKey.empty())
            SecureZeroMemory(work->apiKey.data(), work->apiKey.size() * sizeof(wchar_t));
        work->apiKey.clear();

        // The dialog remains open while busy. If posting nevertheless fails,
        // release the payload here rather than leaking it.
        if (!PostMessageW(work->target, WM_GENERATION_COMPLETE, work->tag,
                          reinterpret_cast<LPARAM>(work))) {
            if (work->threadHandle) CloseHandle(work->threadHandle);
            delete work;
        }
        return 0;
    }


    inline void RequestCancel(WorkItem* work)
    {
        if (!work) return;
        int expectedState = 0;
        if (work->state.compare_exchange_strong(expectedState, 1, std::memory_order_acq_rel)
            && work->threadHandle)
            CancelSynchronousIo(work->threadHandle);
    }

    inline void Destroy(WorkItem* work)
    {
        if (!work) return;
        if (work->threadHandle) CloseHandle(work->threadHandle);
        delete work;
    }

    inline bool Start(
        HWND target,
        std::wstring apiKey,
        std::function<GeminiGenerateResult(const std::wstring&)> operation,
        WPARAM tag = 0,
        WorkItem** outputWork = nullptr)
    {
        if (outputWork) *outputWork = nullptr;
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
        work->threadHandle = thread;
        if (outputWork) *outputWork = work;
        return true;
    }
}
