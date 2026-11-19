#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace MarkdownRenderer
{
    struct Range { size_t start = 0; size_t length = 0; };
    struct Result
    {
        std::wstring text;
        std::vector<Range> bold;
        std::vector<Range> headings;
        std::vector<Range> code;
    };

    inline bool IsHorizontalRule(const std::wstring& line, size_t first)
    {
        size_t count = 0;
        wchar_t marker = 0;
        for (size_t i = first; i < line.size(); ++i)
        {
            const wchar_t ch = line[i];
            if (ch == L' ' || ch == L'\t') continue;
            if (ch != L'-' && ch != L'*' && ch != L'_') return false;
            if (!marker) marker = ch;
            if (marker != ch) return false;
            ++count;
        }
        return count >= 3;
    }

    // Find a matching ** or __ marker outside inline-code spans.
    // A marker inside `...` must not accidentally close an outer bold span.
    inline size_t FindClosingBoldMarker(const std::wstring& line, size_t begin,
        size_t end, wchar_t marker)
    {
        bool inCode = false;
        for (size_t i = begin; i + 1 < end; ++i)
        {
            if (line[i] == static_cast<wchar_t>(96))
            {
                inCode = !inCode;
                continue;
            }
            if (!inCode && line[i] == marker && line[i + 1] == marker)
                return i;
        }
        return std::wstring::npos;
    }

    inline void AppendInline(const std::wstring& line, size_t begin, size_t end,
        Result& result, bool codeLine = false)
    {
        if (codeLine)
        {
            const size_t start = result.text.size();
            result.text.append(line, begin, end - begin);
            if (result.text.size() > start) result.code.push_back({ start, result.text.size() - start });
            return;
        }
        bool bold = false, code = false;
        wchar_t boldMarker = 0;
        size_t boldStart = 0, codeStart = 0;
        for (size_t i = begin; i < end;)
        {
            if (!code && i + 1 < end &&
                ((line[i] == L'*' && line[i + 1] == L'*') ||
                 (line[i] == L'_' && line[i + 1] == L'_')))
            {
                const wchar_t marker = line[i];
                const size_t close = FindClosingBoldMarker(line, i + 2, end, marker);
                if ((!bold && close != std::wstring::npos) || (bold && marker == boldMarker))
                {
                    if (!bold) { bold = true; boldMarker = marker; boldStart = result.text.size(); }
                    else
                    {
                        if (result.text.size() > boldStart)
                            result.bold.push_back({ boldStart, result.text.size() - boldStart });
                        bold = false;
                        boldMarker = 0;
                    }
                    i += 2;
                    continue;
                }
            }
            if (line[i] == static_cast<wchar_t>(96))
            {
                const size_t close = line.find(static_cast<wchar_t>(96), i + 1);
                if ((!code && close != std::wstring::npos && close < end) || code)
                {
                    if (!code) { code = true; codeStart = result.text.size(); }
                    else
                    {
                        if (result.text.size() > codeStart)
                            result.code.push_back({ codeStart, result.text.size() - codeStart });
                        code = false;
                    }
                    ++i;
                    continue;
                }
            }
            result.text += line[i++];
        }
        // Unmatched delimiters are left as literal text; they must not style
        // the remainder of the line accidentally.
    }

    inline Result Render(const std::wstring& source)
    {
        Result result;
        size_t pos = 0;
        bool fencedCode = false;
        while (pos < source.size())
        {
            size_t end = source.find(L'\n', pos);
            if (end == std::wstring::npos) end = source.size();
            size_t lineEnd = end;
            if (lineEnd > pos && source[lineEnd - 1] == L'\r') --lineEnd;
            const std::wstring line = source.substr(pos, lineEnd - pos);
            size_t first = 0;
            while (first < line.size() && (line[first] == L' ' || line[first] == L'\t')) ++first;
            const bool fence = first + 2 < line.size() && line.compare(first, 3, L"```") == 0;
            if (fence) fencedCode = !fencedCode;
            else if (fencedCode) AppendInline(line, 0, line.size(), result, true);
            else if (IsHorizontalRule(line, first))
            {
                const size_t start = result.text.size();
                result.text += L"────────────────────────";
                result.bold.push_back({ start, result.text.size() - start });
            }
            else
            {
                size_t contentStart = first;
                size_t hashes = 0;
                while (contentStart + hashes < line.size() && line[contentStart + hashes] == L'#' && hashes < 6) ++hashes;
                bool heading = hashes > 0 && contentStart + hashes < line.size() && line[contentStart + hashes] == L' ';
                if (heading) contentStart += hashes + 1;

                const size_t lineStart = result.text.size();
                result.text.append(line, 0, heading ? first : contentStart);
                if (!heading && contentStart == first && first + 1 < line.size() &&
                    (line[first] == L'*' || line[first] == L'-' || line[first] == L'+') &&
                    line[first + 1] == L' ')
                {
                    result.text.replace(lineStart + first, 2, L"・ ");
                    contentStart = first + 2;
                }
                else if (!heading && contentStart == first)
                {
                    size_t digits = first;
                    while (digits < line.size() && line[digits] >= L'0' && line[digits] <= L'9') ++digits;
                    if (digits > first && digits + 1 < line.size() &&
                        (line[digits] == L'.' || line[digits] == L')') && line[digits + 1] == L' ')
                    {
                        contentStart = digits + 2;
                        result.text.append(line, first, contentStart - first);
                    }
                }
                const size_t formattedStart = result.text.size();
                AppendInline(line, contentStart, line.size(), result);
                const size_t formattedLength = result.text.size() - formattedStart;
                if (heading && formattedLength > 0)
                    result.headings.push_back({ formattedStart, formattedLength });
            }
            // RichEdit stores a paragraph break as one character (CR). Keeping CRLF
            // here makes every formatting range after the first line drift by one.
            if (end < source.size()) result.text += L"\r";
            pos = end + (end < source.size() ? 1 : 0);
        }
        return result;
    }
}
