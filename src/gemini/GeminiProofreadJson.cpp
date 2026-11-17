#include "GeminiProofreadJson.h"
#include <windows.h>
#include <cstdint>

namespace {
void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7f) out.push_back(static_cast<char>(cp));
    else if (cp <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else if (cp <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    }
}
bool Hex4(const std::string& s, size_t& p, uint32_t& value) {
    if (p + 4 > s.size()) return false;
    value = 0;
    for (int i = 0; i < 4; ++i) {
        const char c = s[p++];
        value <<= 4;
        if (c >= '0' && c <= '9') value |= c - '0';
        else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
        else return false;
    }
    return true;
}
void SkipWs(const std::string& s, size_t& p) {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) ++p;
}
bool ParseString(const std::string& s, size_t& p, std::string& out) {
    if (p >= s.size() || s[p++] != '"') return false;
    out.clear();
    while (p < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[p++]);
        if (c == '"') return true;
        if (c < 0x20) return false;
        if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
        if (p >= s.size()) return false;
        const char e = s[p++];
        switch (e) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
            uint32_t cp = 0;
            if (!Hex4(s, p, cp)) return false;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (p + 2 > s.size() || s[p] != '\\' || s[p + 1] != 'u') return false;
                p += 2;
                uint32_t low = 0;
                if (!Hex4(s, p, low) || low < 0xdc00 || low > 0xdfff) return false;
                cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
            } else if (cp >= 0xdc00 && cp <= 0xdfff) return false;
            AppendUtf8(out, cp);
            break;
        }
        default: return false;
        }
    }
    return false;
}
}
bool ExtractGeminiResponseText(const std::string& json, std::wstring& output) {
    output.clear();
    size_t p = 0;
    while (p < json.size()) {
        if (json[p] != '"') { ++p; continue; }
        std::string key;
        if (!ParseString(json, p, key)) return false;
        size_t afterKey = p;
        SkipWs(json, afterKey);
        if (afterKey >= json.size() || json[afterKey] != ':') continue;
        size_t valuePos = afterKey + 1;
        SkipWs(json, valuePos);
        if (key != "text" || valuePos >= json.size() || json[valuePos] != '"') {
            p = valuePos;
            continue;
        }
        std::string value;
        if (!ParseString(json, valuePos, value) || value.empty()) return false;
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (count <= 0) return false;
        output.resize(static_cast<size_t>(count));
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()), output.data(), count)) {
            output.clear();
            return false;
        }
        return true;
    }
    return false;
}
