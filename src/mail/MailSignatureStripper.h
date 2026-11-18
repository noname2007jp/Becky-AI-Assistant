#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace MailSignature
{
inline std::wstring TrimLine(const std::wstring& value)
{
    const size_t first = value.find_first_not_of(L" \t\r");
    if (first == std::wstring::npos) return {};
    const size_t last = value.find_last_not_of(L" \t\r");
    return value.substr(first, last - first + 1);
}

inline bool IsForwardOrQuoteSeparator(const std::wstring& line)
{
    return line.find(L"Original Message") != std::wstring::npos ||
        line.find(L"Forwarded message") != std::wstring::npos ||
        line.find(L"転送メッセージ") != std::wstring::npos ||
        line.find(L"元のメッセージ") != std::wstring::npos;
}

inline bool ContainsAny(const std::wstring& text, const std::vector<std::wstring>& words);

inline bool IsSignatureSeparator(const std::wstring& raw)
{
    const std::wstring line = TrimLine(raw);
    if (IsForwardOrQuoteSeparator(line)) return false;
    // RFC 5322 separator includes a required trailing space; do not treat an
    // arbitrary two-hyphen line as a signature boundary.
    if (raw == L"-- ") return true;
    if (line.size() < 5) return false;
    for (wchar_t c : line)
        if (c != L'-' && c != L'_' && c != L'=' && c != L'*' &&
            c != L'─' && c != L'━' && c != L'―' && c != L'—' &&
            c != L'＝' && c != L'＊') return false;
    return true;
}

inline bool ContainsAny(const std::wstring& text, const std::vector<std::wstring>& words)
{
    for (const auto& word : words)
        if (text.find(word) != std::wstring::npos) return true;
    return false;
}

// Remove only clearly identifiable trailing signature blocks. Ambiguous prose is retained.
inline std::wstring RemoveObviousSignature(const std::wstring& body)
{
    std::vector<std::wstring> lines;
    std::vector<size_t> lineStarts;
    size_t pos = 0;
    while (pos <= body.size())
    {
        lineStarts.push_back(pos);
        const size_t end = body.find(L'\n', pos);
        std::wstring line = body.substr(pos, end == std::wstring::npos ? body.size() - pos : end - pos);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        lines.push_back(std::move(line));
        if (end == std::wstring::npos) break;
        pos = end + 1;
    }

    // Keep original offsets so CRLF input remains byte-for-character aligned.
    auto prefixBeforeLine = [&](size_t lineIndex) {
        size_t end = lineStarts[lineIndex];
        while (end > 0 && (body[end - 1] == L'\r' || body[end - 1] == L'\n' ||
            body[end - 1] == L' ' || body[end - 1] == L'\t')) --end;
        return body.substr(0, end);
    };

    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (IsSignatureSeparator(lines[i]))
        {
            // Decorative rules are common in ordinary messages. Only remove one
            // when the following tail contains clear signature/contact information.
            const bool rfcDelimiter = lines[i] == L"-- ";
            bool signatureEvidence = false;
            for (size_t j = i + 1; j < lines.size() && j <= i + 10; ++j)
            {
                const std::wstring tailLine = TrimLine(lines[j]);
                if (ContainsAny(tailLine, {L"株式会社", L"有限会社", L"合同会社", L"（株）", L"(株)",
                    L"営業部", L"総務部", L"開発部", L"サポート部", L"TEL", L"Tel", L"電話",
                    L"FAX", L"Fax", L"携帯", L"Email", L"E-mail", L"〒", L"住所", L"所在地",
                    L"http://", L"https://", L"www."}) || tailLine.find(L'@') != std::wstring::npos)
                { signatureEvidence = true; break; }
            }
            if (!rfcDelimiter && !signatureEvidence) continue;
            return prefixBeforeLine(i);
        }
    }

    const size_t tailStart = lines.size() > 12 ? lines.size() - 12 : 0;
    size_t firstSignatureLine = lines.size();
    bool hasIdentity = false;
    bool hasPhone = false, hasEmail = false, hasPostal = false, hasAddress = false, hasUrl = false;
    for (size_t i = tailStart; i < lines.size(); ++i)
    {
        const std::wstring line = TrimLine(lines[i]);
        if (line.empty()) continue;
        if (ContainsAny(line, {L"株式会社", L"有限会社", L"合同会社", L"（株）", L"(株)",
            L"営業部", L"総務部", L"開発部", L"サポート部", L"担当", L"代表", L"部署"}))
        { hasIdentity = true; firstSignatureLine = (std::min)(firstSignatureLine, i); }
        if (ContainsAny(line, {L"TEL", L"Tel", L"電話", L"FAX", L"Fax", L"携帯"})) hasPhone = true;
        if (line.find(L'@') != std::wstring::npos) hasEmail = true;
        if (line.find(L"〒") != std::wstring::npos) hasPostal = true;
        if (ContainsAny(line, {L"住所", L"所在地"})) hasAddress = true;
        if (ContainsAny(line, {L"http://", L"https://", L"www."})) hasUrl = true;
    }
    const int contactKinds = static_cast<int>(hasPhone) + static_cast<int>(hasEmail) +
        static_cast<int>(hasPostal || hasAddress) + static_cast<int>(hasUrl);
    if (hasIdentity && contactKinds >= 2 && firstSignatureLine < lines.size())
    {
        return prefixBeforeLine(firstSignatureLine);
    }
    return body;
}
}
