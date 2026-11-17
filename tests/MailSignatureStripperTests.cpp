#include "mail/MailSignatureStripper.h"
#include <iostream>
#include <string>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
        if (!ok) ++failures;
    };
    const std::wstring rfc = L"ご連絡ありがとうございます。\n確認して回答します。\n\n-- \n山田太郎\nExample株式会社";
    check(MailSignature::RemoveObviousSignature(rfc) == L"ご連絡ありがとうございます。\n確認して回答します。",
        "removes RFC signature block");
    const std::wstring japanese = L"ご依頼の件、承知しました。\n\nExample株式会社\n営業部\nTEL: 03-1234-5678\nEmail: info@example.jp\nhttps://example.jp";
    check(MailSignature::RemoveObviousSignature(japanese) == L"ご依頼の件、承知しました。",
        "removes clearly identifiable Japanese corporate signature without separator");
    const std::wstring quote = L"ご連絡ありがとうございます。\n-----Original Message-----\nFrom: sender@example.jp\n本文";
    check(MailSignature::RemoveObviousSignature(quote) == quote,
        "does not treat original-message separator as signature");
    const std::wstring ambiguous = L"ご連絡ありがとうございます。\n株式会社Exampleについて確認します。";
    check(MailSignature::RemoveObviousSignature(ambiguous) == ambiguous,
        "retains ambiguous prose mentioning a company");
    const std::wstring divider = L"説明の前半です。\n------\n説明の後半です。";
    check(MailSignature::RemoveObviousSignature(divider) == divider,
        "does not remove ordinary text after a decorative divider");
    const std::wstring normal = L"お世話になっております。\n明日までに回答します。";
    check(MailSignature::RemoveObviousSignature(normal) == normal,
        "leaves ordinary message unchanged");
    return failures == 0 ? 0 : 1;
}
