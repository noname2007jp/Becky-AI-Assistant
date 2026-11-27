#pragma once
#include <string>
#include "ApiEndpoints.h"

inline bool IsValidWorkersAIAccountId(const std::wstring& accountId)
{
    return accountId.size() == 32 &&
        accountId.find_first_not_of(L"0123456789abcdefABCDEF") == std::wstring::npos;
}

inline std::wstring BuildWorkersAIChatCompletionsPath(const std::wstring& accountId)
{
    return std::wstring(api_endpoints::kWorkersAIChatPathPrefix) + accountId + api_endpoints::kWorkersAIChatPathSuffix;
}
