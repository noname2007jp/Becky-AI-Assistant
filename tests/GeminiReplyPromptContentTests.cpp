#include "gemini/GeminiReplyPromptContent.h"

#include <iostream>
#include <string>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
        if (!ok) ++failures;
    };

    const std::wstring requirements = L"・納期は12月10日\n・見積書を添付する";
    const std::wstring section =
        GeminiReplyPromptContent::BuildReplyRequirementsSection(requirements);

    check(section.rfind(L"\n\n", 0) == 0,
        "reply requirements section starts with actual line breaks");
    check(section.find(L"\\n") == std::wstring::npos,
        "reply requirements section contains no literal backslash-n sequences");
    check(section.find(requirements) != std::wstring::npos,
        "all user-specified reply requirements are retained");
    check(GeminiReplyPromptContent::BuildReplyRequirementsSection(L"").empty(),
        "empty reply requirements add no section");

    return failures == 0 ? 0 : 1;
}
