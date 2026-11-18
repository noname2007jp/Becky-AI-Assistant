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

    const std::string base64Html =
        "Content-Type: text/html; charset=utf-8\\r\\n"
        "Content-Transfer-Encoding: base64\\r\\n\\r\\n"
        "PGh0bWw+PGJvZHk+PHA+SFRNTCAmYW1wOyBib2R5PC9wPjwvYm9keT48L2h0bWw+";
    Check(Extract(base64Html, output) &&
        output.find(L"HTML & body") != std::wstring::npos,
        "Base64-encoded HTML body");

    const std::string base64Alternative =
        "Content-Type: multipart/alternative; boundary=\\\"b64-boundary\\\"\\r\\n\\r\\n"
        "--b64-boundary\\r\\n"
        "Content-Type: text/plain; charset=utf-8\\r\\n"
        "Content-Transfer-Encoding: base64\\r\\n\\r\\n"
        "UGxhaW4gZW5jb2RlZCBib2R5\\r\\n"
        "--b64-boundary\\r\\n"
        "Content-Type: text/html; charset=utf-8\\r\\n"
        "Content-Transfer-Encoding: base64\\r\\n\\r\\n"
        "PGh0bWw+PGJvZHk+PHA+SFRNTCAmYW1wOyBib2R5PC9wPjwvYm9keT48L2h0bWw+\\r\\n"
        "--b64-boundary--\\r\\n";
    Check(Extract(base64Alternative, output) &&
        output.find(L"Plain encoded body") != std::wstring::npos,
        "multipart alternative with Base64 plain and HTML parts");


    Check(Extract(
        "Content-Type: text/plain; charset=shift_jis\r\n\r\n"
        "\x93\xFA\x96\x7B\x8C\xEA",
        output) && output == L"日本語", "Japanese Shift_JIS / CP932");

    Check(Extract(
        "Content-Type: text/plain; charset=euc-jp\r\n\r\n"
        "\xC6\xFC\xCB\xDC\xB8\xEC",
        output) && output == L"日本語", "Japanese EUC-JP");

    Check(Extract(
        "Content-Type: text/plain; charset=iso-2022-jp\r\n\r\n"
        "\x1B\x24\x42\x46\x7C\x4B\x5C\x38\x6C\x1B\x28\x42",
        output) && output == L"日本語", "Japanese ISO-2022-JP");

    const std::string nested =
        "Content-Type: multipart/mixed; boundary=outer\r\n\r\n"
        "--outer\r\n"
        "Content-Type: multipart/alternative; boundary=inner\r\n\r\n"
        "--inner\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n"
        "Nested plain text\r\n"
        "--inner\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"
        "<p>Nested HTML</p>\r\n"
        "--inner--\r\n"
        "--outer\r\nContent-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename=ignored.bin\r\n\r\n"
        "ignored attachment\r\n"
        "--outer--\r\n";
    Check(Extract(nested, output) && output == L"Nested plain text",
        "nested multipart prefers plain text and ignores attachments");

    const std::string longBody(100050, 'A');
    const std::string longMail =
        "Content-Type: text/plain; charset=utf-8\r\n\r\n" + longBody;
    Check(Extract(longMail, output) && output.size() == 100000,
        "long message body is safely capped at 100,000 characters");

    {
        const std::string oversized(5 * 1024 * 1024 + 1, 'x');
        std::wstring error;
        Check(!ExtractMailText(oversized.data(), oversized.size(), output, error) &&
              error.find(L"5 MiB") != std::wstring::npos,
              "raw MIME message above 5 MiB is rejected with a clear error");
    }

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
