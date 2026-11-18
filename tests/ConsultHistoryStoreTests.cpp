#include "ui/ConsultHistoryStore.h"

#include <cassert>
#include <string>

int main()
{
    using ConsultHistoryStore::Snapshot;

    Snapshot first;
    first.history.push_back({ false, L"initial prompt" });
    first.history.push_back({ true, L"first answer" });
    first.transcript = L"【第1回】\r\n回答";
    first.roundCount = 1;
    ConsultHistoryStore::Save(L"mail-001", first);

    Snapshot restored;
    assert(ConsultHistoryStore::Load(L"mail-001", restored));
    assert(restored.history.size() == 2);
    assert(restored.history[0].text == L"initial prompt");
    assert(restored.history[1].isModel);
    assert(restored.transcript == first.transcript);
    assert(restored.roundCount == 1);

    Snapshot second;
    second.history.push_back({ false, L"follow-up" });
    second.history.push_back({ true, L"second answer" });
    second.transcript = L"【第2回】";
    second.roundCount = 2;
    ConsultHistoryStore::Save(L"mail-001", second);

    Snapshot updated;
    assert(ConsultHistoryStore::Load(L"mail-001", updated));
    assert(updated.roundCount == 2);
    assert(updated.transcript == L"【第2回】");

    ConsultHistoryStore::Clear(L"mail-001");
    Snapshot cleared;
    assert(!ConsultHistoryStore::Load(L"mail-001", cleared));

    for (unsigned int i = 0; i < ConsultHistoryStore::kMaximumSessions + 1; ++i)
    {
        Snapshot item;
        item.history.push_back({ false, L"question" });
        item.transcript = L"answer";
        ConsultHistoryStore::Save(L"mail-" + std::to_wstring(i), item);
    }
    Snapshot oldest;
    assert(!ConsultHistoryStore::Load(L"mail-0", oldest));
    Snapshot newest;
    assert(ConsultHistoryStore::Load(
        L"mail-" + std::to_wstring(ConsultHistoryStore::kMaximumSessions), newest));

    return 0;
}
