#include "MimeTextExtractor.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace
{
constexpr size_t kMaxDecodedBytes = 2 * 1024 * 1024;

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string Trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

struct Part
{
    std::string headers;
    std::string body;
    std::string contentType = "text/plain";
    std::string charset;
    std::string transferEncoding;
    std::string disposition;
    std::string boundary;
};

bool SplitHeaderBody(std::string_view raw, std::string& headers, std::string& body)
{
    size_t p = raw.find("\r\n\r\n");
    size_t sep = 4;
    if (p == std::string_view::npos) { p = raw.find("\n\n"); sep = 2; }
    if (p == std::string_view::npos) return false;
    headers.assign(raw.substr(0, p));
    body.assign(raw.substr(p + sep));
    return true;
}

std::string HeaderValue(const std::string& headers, const std::string& wanted)
{
    std::string currentName, currentValue;
    auto finish = [&]() -> std::string {
        if (Lower(currentName) == Lower(wanted)) return Trim(currentValue);
        return {};
    };

    size_t pos = 0;
    while (pos <= headers.size())
    {
        size_t end = headers.find('\n', pos);
        std::string line = headers.substr(pos,
            end == std::string::npos ? headers.size() - pos : end - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && (line[0] == ' ' || line[0] == '\t'))
        {
            if (!currentName.empty()) currentValue += " " + Trim(line);
        }
        else
        {
            std::string value = finish();
            if (!value.empty()) return value;
            currentName.clear(); currentValue.clear();
            size_t colon = line.find(':');
            if (colon != std::string::npos)
            {
                currentName = Trim(line.substr(0, colon));
                currentValue = Trim(line.substr(colon + 1));
            }
        }
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return finish();
}

std::string Parameter(const std::string& value, const std::string& name)
{
    const std::string lower = Lower(value);
    const std::string key = Lower(name) + "=";
    size_t p = lower.find(key);
    if (p == std::string::npos) return {};
    p += key.size();
    while (p < value.size() && (value[p] == ' ' || value[p] == '\t')) ++p;
    if (p < value.size() && value[p] == '"')
    {
        size_t e = value.find('"', p + 1);
        if (e == std::string::npos) return value.substr(p + 1);
        return value.substr(p + 1, e - p - 1);
    }
    size_t e = value.find(';', p);
    return Trim(value.substr(p, e == std::string::npos ? e : e - p));
}

bool ParsePart(std::string_view raw, Part& part)
{
    if (!SplitHeaderBody(raw, part.headers, part.body)) return false;
    std::string ct = HeaderValue(part.headers, "Content-Type");
    if (!ct.empty())
    {
        size_t semi = ct.find(';');
        part.contentType = Lower(Trim(ct.substr(0, semi)));
        part.charset = Parameter(ct, "charset");
        part.boundary = Parameter(ct, "boundary");
    }
    part.transferEncoding = Lower(HeaderValue(part.headers, "Content-Transfer-Encoding"));
    part.disposition = Lower(HeaderValue(part.headers, "Content-Disposition"));
    return true;
}

std::string DecodeBase64(const std::string& input)
{
    static const signed char invalid = -1;
    static signed char table[256];
    static bool initialized = false;
    if (!initialized)
    {
        std::fill(std::begin(table), std::end(table), invalid);
        const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; alphabet[i]; ++i) table[static_cast<unsigned char>(alphabet[i])] = static_cast<signed char>(i);
        initialized = true;
    }
    std::string out;
    out.reserve(input.size() * 3 / 4);
    unsigned int accumulator = 0; int bits = 0;
    for (unsigned char c : input)
    {
        if (c == '=') break;
        signed char v = table[c];
        if (v < 0) continue;
        accumulator = (accumulator << 6) | static_cast<unsigned int>(v);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<char>((accumulator >> bits) & 0xff));
            if (out.size() > kMaxDecodedBytes) break;
        }
    }
    return out;
}

std::string DecodeQuotedPrintable(const std::string& input)
{
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size() && out.size() <= kMaxDecodedBytes; ++i)
    {
        if (input[i] == '=' && i + 1 < input.size())
        {
            if (input[i + 1] == '\n') { ++i; continue; }
            if (input[i + 1] == '\r' && i + 2 < input.size() && input[i + 2] == '\n') { i += 2; continue; }
            if (i + 2 < input.size())
            {
                int a = hex(input[i + 1]), b = hex(input[i + 2]);
                if (a >= 0 && b >= 0) { out.push_back(static_cast<char>((a << 4) | b)); i += 2; continue; }
            }
        }
        out.push_back(input[i]);
    }
    return out;
}

std::wstring ToWide(const std::string& bytes, const std::string& charset)
{
    UINT cp = CP_ACP;
    const std::string cs = Lower(charset);
    if (cs == "utf-8" || cs == "utf8") cp = CP_UTF8;
    else if (cs == "shift_jis" || cs == "shift-jis" || cs == "sjis" || cs == "windows-31j" || cs == "cp932") cp = 932;
    else if (cs == "euc-jp" || cs == "euc_jp") cp = 51932;
    else if (cs == "iso-2022-jp") cp = 50220;
    else if (cs == "us-ascii" || cs == "ascii") cp = 20127;

    int count = MultiByteToWideChar(cp, 0, bytes.data(),
        static_cast<int>(std::min(bytes.size(), kMaxDecodedBytes)), nullptr, 0);
    if (count <= 0 && cp != CP_ACP)
    {
        cp = CP_ACP;
        count = MultiByteToWideChar(cp, 0, bytes.data(),
            static_cast<int>(std::min(bytes.size(), kMaxDecodedBytes)), nullptr, 0);
    }
    if (count <= 0) return L"(本文の文字コードを変換できません)";
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(cp, 0, bytes.data(),
        static_cast<int>(std::min(bytes.size(), kMaxDecodedBytes)), result.data(), count);
    return result;
}

void StripHtml(std::string& text)
{
    std::string out;
    out.reserve(text.size());
    bool inTag = false;
    for (char c : text)
    {
        if (c == '<') { inTag = true; continue; }
        if (c == '>') { inTag = false; out.push_back(' '); continue; }
        if (!inTag) out.push_back(c);
    }
    text.swap(out);
}

bool ExtractPart(std::string_view raw, std::wstring& result, bool& htmlFound, std::wstring& htmlText, int depth)
{
    if (depth > 8) return false;
    Part part;
    if (!ParsePart(raw, part)) return false;

    if (part.contentType.rfind("multipart/", 0) == 0 && !part.boundary.empty())
    {
        const std::string marker = "--" + part.boundary;
        size_t pos = 0;
        std::wstring htmlCandidate;
        while ((pos = part.body.find(marker, pos)) != std::string::npos)
        {
            pos += marker.size();
            if (part.body.compare(pos, 2, "--") == 0) break;
            if (part.body.compare(pos, 2, "\r\n") == 0) pos += 2;
            else if (pos < part.body.size() && part.body[pos] == '\n') ++pos;
            size_t next = part.body.find(marker, pos);
            if (next == std::string::npos) break;
            std::string_view child(part.body.data() + pos, next - pos);
            std::wstring childResult;
            if (ExtractPart(child, childResult, htmlFound, htmlCandidate, depth + 1))
            {
                if (!childResult.empty()) { result = std::move(childResult); return true; }
            }
            pos = next;
        }
        if (!htmlCandidate.empty()) { htmlText = std::move(htmlCandidate); htmlFound = true; }
        return false;
    }

    if (part.disposition.find("attachment") != std::string::npos) return false;
    std::string decoded = part.body;
    if (part.transferEncoding == "base64") decoded = DecodeBase64(decoded);
    else if (part.transferEncoding == "quoted-printable") decoded = DecodeQuotedPrintable(decoded);
    if (decoded.size() > kMaxDecodedBytes) decoded.resize(kMaxDecodedBytes);

    if (part.contentType == "text/plain" || part.contentType.empty())
    {
        result = ToWide(decoded, part.charset);
        return !result.empty();
    }
    if (part.contentType == "text/html")
    {
        StripHtml(decoded);
        htmlText = ToWide(decoded, part.charset);
        htmlFound = true;
    }
    return false;
}
}

bool ExtractMailText(const char* source, size_t sourceLength,
                     std::wstring& output, std::wstring& error)
{
    output.clear();
    error.clear();
    if (source == nullptr || sourceLength == 0)
    {
        error = L"メール本文が空です。";
        return false;
    }
    if (sourceLength > 5 * 1024 * 1024)
    {
        error = L"メールサイズが上限（5 MiB）を超えています。";
        return false;
    }

    std::string raw(source, sourceLength);
    bool htmlFound = false;
    std::wstring htmlText;
    if (!ExtractPart(raw, output, htmlFound, htmlText, 0))
    {
        if (htmlFound) output = std::move(htmlText);
    }
    if (output.empty())
    {
        error = L"表示可能なテキスト本文が見つかりませんでした。HTMLのみ、暗号化、または未対応の形式の可能性があります。";
        return false;
    }
    if (output.size() > 100000) output.resize(100000);
    return true;
}
