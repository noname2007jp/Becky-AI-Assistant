#include "MimeTextExtractor.h"

#include <iostream>
#include <string>

namespace
{
int failures = 0;

void Check(bool condition, const char* name)
{
    if (condition) std::cout << "[PASS] " << name << "\n";
    else { std::cerr << "[FAIL] " << name << "\n"; ++failures; }
}

bool Extract(const std::string& raw, std::wstring& output)
{
    std::wstring error;
    return ExtractMailText(raw.data(), raw.size(), output, error);
}
}

int main()
{
    std::wstring output;

    Check(Extract(
        "From: test@example.com\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nHello\r\nWorld",
        output) && output == L"Hello\r\nWorld", "plain UTF-8 text");

    Check(Extract(
        "From: test@example.com\r\nContent-Type: text/plain; charset=utf-8\r\n"
        "Content-Transfer-Encoding: quoted-printable\r\n\r\n"
        "caf=C3=A9=0D=0Asecond line",
        output) && output == L"café\r\nsecond line", "quoted-printable UTF-8");

    Check(Extract(
        "From: test@example.com\r\nContent-Type: text/plain; charset=utf-8\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\nSGVsbG8=",
        output) && output == L"Hello", "base64 text");

    const std::string alternative =
        "From: test@example.com\r\n"
        "Content-Type: multipart/alternative; boundary=\"mix-boundary\"\r\n\r\n"
        "--mix-boundary\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nPlain version\r\n"
        "--mix-boundary\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"
        "<p>HTML version</p>\r\n--mix-boundary--\r\n";
    Check(Extract(alternative, output) && output == L"Plain version", "multipart alternative prefers plain text");

    const std::string html =
        "From: test@example.com\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"
        "<html><body><p>Hello &amp; welcome&nbsp;!</p>"
        "<div>Line &#50; &#x33;</div><ul><li>First</li><li>Second</li></ul></body></html>";
    Check(Extract(html, output) &&
        output.find(L"Hello & welcome\u00a0!") != std::wstring::npos &&
        output.find(L"Line 2 3") != std::wstring::npos &&
        output.find(L"\r\n・First") != std::wstring::npos &&
        output.find(L"\r\n・Second") != std::wstring::npos,
        "HTML entities and structural line breaks");

    const std::string attachment =
        "From: test@example.com\r\nContent-Type: application/octet-stream; name=\"file.bin\"\r\n"
        "Content-Disposition: attachment; filename=\"file.bin\"\r\n\r\nignored";
    Check(!Extract(attachment, output), "attachment-only message rejected");

    Check(!Extract("", output), "empty message rejected");

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All MIME extraction tests passed.\n";
    return 0;
}
