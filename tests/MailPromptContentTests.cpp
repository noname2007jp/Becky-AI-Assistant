#include "mail/MailPromptContent.h"
#include <iostream>
#include <string>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
        if (!ok) ++failures;
    };

    const std::wstring body =
        L"お世話になっております。\nご依頼の件、確認します。\n\n-- \n"
        L"株式会社Example\n営業部\nTEL: 03-1234-5678\ninfo@example.jp";
    const std::wstring context = MailPromptContent::BuildEmailContext(L"見積の件", body);
    check(context.find(L"【差出人】") == std::wstring::npos,
        "email context has no sender field");
    check(context.find(L"info@example.jp") == std::wstring::npos,
        "obvious signature contact is excluded");
    check(context.find(L"ご依頼の件、確認します。") != std::wstring::npos,
        "message body is retained");
    check(context.find(L"見積の件") != std::wstring::npos,
        "subject is retained");

    const std::wstring ordinary =
        L"説明の前半です。\n------\n説明の後半です。";
    check(MailPromptContent::BuildEmailContext(L"説明", ordinary).find(ordinary) != std::wstring::npos,
        "ordinary divider and following prose are retained");

    return failures == 0 ? 0 : 1;
}
