#include "gemini/GeminiTextContinuation.h"
#include <iostream>
#include <string>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        if (ok) std::cout << "[PASS] " << name << "\n";
        else { std::cerr << "[FAIL] " << name << "\n"; ++failures; }
    };
    {
        std::wstring text = L"回答の途中です";
        AppendGeminiContinuationText(text, L"途中です。続きです");
        check(text == L"回答の途中です。続きです",
            "removes exact repeated suffix at continuation boundary");
    }
    {
        std::wstring text = L"最初の回答";
        AppendGeminiContinuationText(text, L"続きの回答");
        check(text == L"最初の回答続きの回答",
            "appends continuation when there is no overlap");
    }
    {
        std::wstring text = L"abc123";
        AppendGeminiContinuationText(text, L"123xyz", 3);
        check(text == L"abc123xyz",
            "honors the maximum overlap length");
    }
    {
        std::wstring text;
        AppendGeminiContinuationText(text, L"最初の回答");
        check(text == L"最初の回答",
            "handles an empty accumulated response");
    }
    return failures ? 1 : 0;
}
