#pragma once
#include <cstddef>
#include <string>

inline void AppendGeminiContinuationText(std::wstring& accumulated,
    const std::wstring& next, std::size_t maxOverlap = 1024)
{
    std::size_t overlap = 0;
    std::size_t limit = accumulated.size();
    if (limit > next.size()) limit = next.size();
    if (limit > maxOverlap) limit = maxOverlap;

    for (std::size_t length = limit; length > 0; --length)
    {
        if (accumulated.compare(accumulated.size() - length, length,
            next, 0, length) == 0)
        {
            overlap = length;
            break;
        }
    }
    accumulated.append(next.substr(overlap));
}
