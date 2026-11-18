#include "ui/ConsultHistoryStore.h"

#include <cassert>
#include <string>

int main()
{
    using ConsultHistoryStore::Conversation;
    using ConsultHistoryStore::Snapshot;

    Conversation first;
    first.id = L"c-1";
    first.title = L"最初の相談";
    first.snapshot.history.push_back({ false, L"initial prompt" });
    first.snapshot.history.push_back({ true, L"first answer" });
    first.snapshot.transcript = L"【第1回】\r\n回答";
    first.snapshot.roundCount = 1;
    first.snapshot.lastQuestion = L"最初の質問";
    ConsultHistoryStore::Save(L"mail-001", first);

    Conversation second;
    second.id = L"c-2";
    second.title = L"別件の確認";
    second.snapshot.history.push_back({ false, L"second question" });
    second.snapshot.history.push_back({ true, L"second answer" });
    second.snapshot.transcript = L"【第1回】\r\n別の回答";
    second.snapshot.roundCount = 1;
    ConsultHistoryStore::Save(L"mail-001", second);

    auto sessions = ConsultHistoryStore::LoadAll(L"mail-001");
    assert(sessions.size() == 2);
    assert(sessions[0].id == L"c-1");
    assert(sessions[0].title == L"最初の相談");
    assert(sessions[0].snapshot.history.size() == 2);
    assert(sessions[0].snapshot.history[0].text == L"initial prompt");
    assert(sessions[0].snapshot.history[1].isModel);
    assert(sessions[0].snapshot.transcript == first.snapshot.transcript);
    assert(sessions[0].snapshot.lastQuestion == L"最初の質問");

    // Updating one conversation must not overwrite its sibling.
    first.snapshot.roundCount = 2;
    first.snapshot.lastQuestion = L"更新した質問";
    first.snapshot.transcript = L"【第2回】";
    ConsultHistoryStore::Save(L"mail-001", first);
    sessions = ConsultHistoryStore::LoadAll(L"mail-001");
    assert(sessions.size() == 2);
    assert(sessions[0].snapshot.roundCount == 2);
    assert(sessions[0].snapshot.lastQuestion == L"更新した質問");
    assert(sessions[1].snapshot.transcript == second.snapshot.transcript);

    ConsultHistoryStore::Clear(L"mail-001", L"c-1");
    sessions = ConsultHistoryStore::LoadAll(L"mail-001");
    assert(sessions.size() == 1 && sessions[0].id == L"c-2");
    ConsultHistoryStore::ClearAll(L"mail-001");
    assert(ConsultHistoryStore::LoadAll(L"mail-001").empty());

    // Keep only the newest conversations for one mail.
    for (unsigned int i = 0; i < ConsultHistoryStore::kMaximumConversationsPerMail + 1; ++i)
    {
        Conversation item;
        item.id = L"session-" + std::to_wstring(i);
        item.title = L"質問";
        item.snapshot.history.push_back({ false, L"question" });
        item.snapshot.transcript = L"answer";
        ConsultHistoryStore::Save(L"mail-limit", item);
    }
    sessions = ConsultHistoryStore::LoadAll(L"mail-limit");
    assert(sessions.size() == ConsultHistoryStore::kMaximumConversationsPerMail);
    assert(sessions.front().id == L"session-1");
    assert(sessions.back().id == L"session-10");

    // Keep the number of mail items bounded as well.
    for (unsigned int i = 0; i < ConsultHistoryStore::kMaximumMailItems + 1; ++i)
    {
        Conversation item;
        item.id = L"only";
        item.snapshot.history.push_back({ false, L"question" });
        item.snapshot.transcript = L"answer";
        ConsultHistoryStore::Save(L"mail-" + std::to_wstring(i), item);
    }
    assert(ConsultHistoryStore::LoadAll(L"mail-0").empty());
    assert(!ConsultHistoryStore::LoadAll(
        L"mail-" + std::to_wstring(ConsultHistoryStore::kMaximumMailItems)).empty());

    return 0;
}
