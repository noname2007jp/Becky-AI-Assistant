#pragma once

#include <string>

namespace WorkersAIReplyFormatting
{
inline bool IsClosingPunctuation(wchar_t c)
{
    return c == L'」' || c == L'』' || c == L'）' || c == L'】' ||
        c == L'〕' || c == L'〉' || c == L'》' || c == L'”' || c == L'’' ||
        c == L'"';
}

inline bool IsSentenceTerminator(wchar_t c)
{
    return c == L'。' || c == L'！' || c == L'？';
}

// Preserve existing paragraphs while splitting sentence runs that an API
// returns as one long line. Always return Windows CRLF line endings.
inline std::wstring EnsureReadableLineBreaks(const std::wstring& text)
{
    std::wstring output;
    output.reserve(text.size() + 32);

    for (size_t i = 0; i < text.size(); ++i)
    {
        const wchar_t c = text[i];
        if (c == L'\r' || c == L'\n')
        {
            if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n')
                ++i;
            output += L"\r\n";
            continue;
        }

        output.push_back(c);
        if (!IsSentenceTerminator(c))
            continue;

        size_t next = i + 1;
        while (next < text.size() && IsClosingPunctuation(text[next]))
        {
            output.push_back(text[next]);
            ++next;
            i = next - 1;
        }

        if (next >= text.size() || text[next] == L'\r' || text[next] == L'\n')
            continue;

        while (next < text.size() &&
            (text[next] == L' ' || text[next] == L'\t' || text[next] == L'　'))
            ++next;

        if (next < text.size() && text[next] != L'\r' && text[next] != L'\n')
        {
            output += L"\r\n";
            i = next - 1;
        }
    }

    return output;
}
}
