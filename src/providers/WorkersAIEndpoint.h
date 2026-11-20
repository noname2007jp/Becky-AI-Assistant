#pragma once
#include <string>

inline bool IsValidWorkersAIAccountId(const std::wstring& accountId)
{
    return accountId.size() == 32 &&
        accountId.find_first_not_of(L"0123456789abcdefABCDEF") == std::wstring::npos;
}

inline std::wstring BuildWorkersAIChatCompletionsPath(const std::wstring& accountId)
{
    return L"/client/v4/accounts/" + accountId + L"/ai/v1/chat/completions";
}
