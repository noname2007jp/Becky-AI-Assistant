#pragma once

#include "gemini/GeminiGenerate.h"

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace ConsultHistoryStore
{
    struct Snapshot
    {
        std::vector<GeminiChatTurn> history;
        std::wstring transcript;
        unsigned int roundCount = 0;
    };

    struct Conversation
    {
        std::wstring id;
        std::wstring title;
        Snapshot snapshot;
    };

    inline std::map<std::wstring, std::vector<Conversation>>& Entries()
    {
        static std::map<std::wstring, std::vector<Conversation>> entries;
        return entries;
    }

    inline std::deque<std::wstring>& MailOrder()
    {
        static std::deque<std::wstring> order;
        return order;
    }

    inline unsigned long long& NextId()
    {
        static unsigned long long value = 1;
        return value;
    }

    inline constexpr size_t kMaximumMailItems = 10;
    inline constexpr size_t kMaximumConversationsPerMail = 10;

    inline std::vector<Conversation> LoadAll(const std::wstring& mailId)
    {
        if (mailId.empty()) return {};
        auto& entries = Entries();
        const auto it = entries.find(mailId);
        if (it == entries.end()) return {};
        auto& order = MailOrder();
        order.erase(std::remove(order.begin(), order.end(), mailId), order.end());
        order.push_back(mailId);
        return it->second;
    }

    inline std::wstring CreateId()
    {
        return L"consult-" + std::to_wstring(NextId()++);
    }

    inline void Clear(const std::wstring& mailId, const std::wstring& conversationId)
    {
        if (mailId.empty() || conversationId.empty()) return;
        auto& entries = Entries();
        const auto it = entries.find(mailId);
        if (it == entries.end()) return;
        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(),
            [&](const Conversation& item) { return item.id == conversationId; }), list.end());
        if (list.empty()) entries.erase(it);
    }

    inline void ClearAll(const std::wstring& mailId)
    {
        if (mailId.empty()) return;
        Entries().erase(mailId);
        auto& order = MailOrder();
        order.erase(std::remove(order.begin(), order.end(), mailId), order.end());
    }

    inline void Save(const std::wstring& mailId, const Conversation& conversation)
    {
        if (mailId.empty() || conversation.id.empty()) return;
        if (conversation.snapshot.history.empty() && conversation.snapshot.transcript.empty())
        {
            Clear(mailId, conversation.id);
            return;
        }

        auto& entries = Entries();
        auto& list = entries[mailId];
        const auto it = std::find_if(list.begin(), list.end(),
            [&](const Conversation& item) { return item.id == conversation.id; });
        if (it == list.end()) list.push_back(conversation);
        else *it = conversation;

        if (list.size() > kMaximumConversationsPerMail)
            list.erase(list.begin(), list.begin() + (list.size() - kMaximumConversationsPerMail));

        auto& order = MailOrder();
        order.erase(std::remove(order.begin(), order.end(), mailId), order.end());
        order.push_back(mailId);
        while (order.size() > kMaximumMailItems)
        {
            entries.erase(order.front());
            order.pop_front();
        }
    }
}
