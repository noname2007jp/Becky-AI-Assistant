#pragma once

#include "gemini/GeminiGenerate.h"

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <utility>

namespace ConsultHistoryStore
{
    struct Snapshot
    {
        std::vector<GeminiChatTurn> history;
        std::wstring transcript;
        unsigned int roundCount = 0;
    };

    inline std::map<std::wstring, Snapshot>& Entries()
    {
        static std::map<std::wstring, Snapshot> entries;
        return entries;
    }

    inline std::deque<std::wstring>& Order()
    {
        static std::deque<std::wstring> order;
        return order;
    }

    inline constexpr size_t kMaximumSessions = 10;

    inline bool Load(const std::wstring& mailId, Snapshot& snapshot)
    {
        if (mailId.empty()) return false;
        auto& entries = Entries();
        const auto it = entries.find(mailId);
        if (it == entries.end()) return false;
        snapshot = it->second;
        auto& order = Order();
        order.erase(std::remove(order.begin(), order.end(), mailId), order.end());
        order.push_back(mailId);
        return true;
    }

    inline void Clear(const std::wstring& mailId)
    {
        if (mailId.empty()) return;
        Entries().erase(mailId);
        auto& order = Order();
        order.erase(std::remove(order.begin(), order.end(), mailId), order.end());
    }

    inline void Save(const std::wstring& mailId, const Snapshot& snapshot)
    {
        if (mailId.empty()) return;
        if (snapshot.history.empty() && snapshot.transcript.empty())
        {
            Clear(mailId);
            return;
        }

        auto& entries = Entries();
        auto& order = Order();
        entries[mailId] = snapshot;
        order.erase(std::remove(order.begin(), order.end(), mailId), order.end());
        order.push_back(mailId);

        while (order.size() > kMaximumSessions)
        {
            entries.erase(order.front());
            order.pop_front();
        }
    }
}
