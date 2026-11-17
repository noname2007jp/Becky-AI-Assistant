#include "gemini/PiiMasker.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void Check(bool condition, const char* name)
{
    if (condition) std::cout << "[PASS] " << name << "\n";
    else { std::cerr << "[FAIL] " << name << "\n"; ++failures; }
}

PiiMaskOptions AllEnabled()
{
    PiiMaskOptions options;
    options.enabled = true;
    options.email = true;
    options.phone = true;
    options.postal = true;
    options.ip = true;
    options.company = false;
    options.person = false;
    options.address = true;
    options.account = true;
    options.custom = true;
    return options;
}
}

int main()
{
    {
        PiiMaskOptions options;
        options.enabled = false;
        PiiMasker masker(options, {});
        const std::wstring source = L"Contact test@example.com or 090-1234-5678";
        Check(masker.IsReady() && !masker.IsEnabled() && masker.Mask(source) == source,
            "masking disabled leaves input unchanged");
    }

    {
        PiiMasker masker(AllEnabled(), {});
        const std::wstring source =
            L"Email: test@example.com / test@example.com\n"
            L"Phone: 090-1234-5678\nPostal: 100-0001\nIP: 192.168.1.10\n"
            L"Address: 東京都千代田区1-2\nAccount: 口座番号: 1234-5678";
        const std::wstring masked = masker.Mask(source);
        Check(masked.find(L"test@example.com") == std::wstring::npos,
            "email addresses are masked");
        Check(masked.find(L"090-1234-5678") == std::wstring::npos,
            "phone numbers are masked");
        Check(masked.find(L"100-0001") == std::wstring::npos,
            "postal codes are masked");
        Check(masked.find(L"192.168.1.10") == std::wstring::npos,
            "IPv4 addresses are masked");
        Check(masked.find(L"東京都千代田区1-2") == std::wstring::npos,
            "labelled addresses are masked");
        Check(masked.find(L"1234-5678") == std::wstring::npos,
            "labelled account numbers are masked");
        Check(masked.find(L"[EMAIL_1]") != std::wstring::npos &&
              masked.find(L"[EMAIL_1]", masked.find(L"[EMAIL_1]") + 1) != std::wstring::npos,
            "identical values reuse the same replacement token");
        Check(masker.Restore(masked) == source,
            "all masked values restore to the original text");
        Check(masker.ContainsAllTokens(masked),
            "all masked placeholders are present in generated text");
        Check(!masker.ContainsAllTokens(L"content with no placeholders"),
            "missing masked placeholders are detected");
    }

    {
        PiiMasker masker(AllEnabled(), { L"Confidential Project", L"Acme" });
        const std::wstring source = L"Acme is part of Confidential Project.";
        const std::wstring masked = masker.Mask(source);
        Check(masked.find(L"Acme") == std::wstring::npos &&
              masked.find(L"Confidential Project") == std::wstring::npos,
            "custom terms are masked");
        Check(masker.Restore(masked) == source,
            "custom terms restore to original text");
    }

    {
        PiiMasker masker(AllEnabled(), {});
        std::wstring preview = L"Please contact Becky Support for details.";
        const size_t start = preview.find(L"Becky Support");
        Check(masker.MaskSelected(preview, start, std::wstring(L"Becky Support").size()),
            "manual selection can be masked");
        Check(preview.find(L"Becky Support") == std::wstring::npos,
            "manual masking updates the preview");
        Check(masker.Restore(preview) == L"Please contact Becky Support for details.",
            "manual masking restores original text");
        Check(!masker.MaskSelected(preview, preview.size() + 1, 2),
            "invalid selection range is rejected");
    }

    {
        PiiMasker masker(AllEnabled(), {});
        const std::wstring source =
            L"Instructions\\nSubject: test\\nFrom: person@example.com\\nBody: confidential text";
        std::wstring preview = masker.Mask(source);
        Check(masker.MaskSelected(preview, 0, preview.size()),
            "manual mask can select the complete automatically masked preview");
        Check(preview.find(L"[MANUAL_") == 0 &&
              preview.find(L"Instructions") == std::wstring::npos,
            "complete preview is represented by one manual token");
        const std::wstring request = masker.Mask(source);
        Check(request == preview,
            "the actual request receives the same whole-selection manual token");
        Check(masker.Restore(request) == source,
            "whole-selection manual token restores to original content");
    }

    {
        PiiMasker masker(AllEnabled(), {});
        const std::wstring content =
            L"Subject: test\\nFrom: person@example.com\\nBody: confidential text";
        std::wstring preview = masker.Mask(content);
        Check(masker.MaskSelected(preview, 0, preview.size()),
            "manual mask can select the complete email content preview");
        const std::wstring fullPrompt = L"Reply instructions\\n\\n" + content;
        const std::wstring request = masker.Mask(fullPrompt);
        Check(request.find(L"Reply instructions") == 0 &&
              request.find(L"Subject:") == std::wstring::npos &&
              request.find(L"confidential text") == std::wstring::npos &&
              request.find(L"[MANUAL_") != std::wstring::npos,
            "masking all email content preserves instructions but hides the full email");
        Check(masker.Restore(request) == fullPrompt,
            "masked email content can be restored after generation");
    }

    {
        PiiMaskOptions options = AllEnabled();
        options.email = false;
        PiiMasker masker(options, {});
        const std::wstring source = L"test@example.com and 090-1234-5678";
        const std::wstring masked = masker.Mask(source);
        Check(masked.find(L"test@example.com") != std::wstring::npos &&
              masked.find(L"090-1234-5678") == std::wstring::npos,
            "individual masking categories can be disabled");
    }

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All PII masking tests passed.\n";
    return 0;
}
