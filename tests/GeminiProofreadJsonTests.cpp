#include "gemini/GeminiProofreadJson.h"
#include <iostream>
#include <string>

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        if (ok) std::cout << "[PASS] " << name << "\n";
        else { std::cerr << "[FAIL] " << name << "\n"; ++failures; }
    };
    {
        std::wstring text;
        const std::string json = R"({"candidates":[{"content":{"parts":[{"text":"株式会社テスト\r\n> 引用文\n校正済み"}]}}]})";
        check(ExtractGeminiResponseText(json, text) &&
              text == L"株式会社テスト\r\n> 引用文\n校正済み",
              "preserves Japanese text, CRLF, LF and greater-than characters");
    }
    {
        std::wstring text;
        const std::string json = R"({"candidates":[{"content":{"parts":[{"text":"\u003e \u682a\u5f0f\u4f1a\u793e\n\uD83D\uDE00"}]}}]})";
        check(ExtractGeminiResponseText(json, text) &&
              text == L"> 株式会社\n\U0001F600",
              "decodes unicode escapes and UTF-16 surrogate pairs");
    }
    {
        std::wstring text;
        const std::string json = R"({"candidates":[{"content":{"parts":[{"text":"literal \\u003e"}]}}]})";
        check(ExtractGeminiResponseText(json, text) &&
              text == L"literal \\u003e",
              "does not double-decode escaped backslashes");
    }
    {
        std::wstring text;
        check(!ExtractGeminiResponseText(R"({"candidates":[{"content":{"parts":[{"text":"bad\uD800"}]}}]})", text),
              "rejects invalid surrogate sequences");
    }
    return failures ? 1 : 0;
}
