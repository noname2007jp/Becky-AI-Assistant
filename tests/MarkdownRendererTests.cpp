#include "ui/MarkdownRenderer.h"
#include <iostream>
#include <string>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
        if (!ok) ++failures;
    };
    const std::wstring markdown =
        L"### 主な不審点・根拠\r\n"
        L"* **不審なURL:** 公式URLとは異なります。\r\n"
        L"* **宛名:** `ogata 様` となっています。\r\n"
        L"---\r\n"
        L"1. URLをクリックしない\r\n"
        L"2. 個人情報を入力しない\r\n";
    const auto rendered = MarkdownRenderer::Render(markdown);
    check(rendered.text.find(L"###") == std::wstring::npos, "heading markers are removed");
    check(rendered.text.find(L"**") == std::wstring::npos, "bold markers are removed");
    check(rendered.text.find(L"* ") == std::wstring::npos, "unordered list markers are converted");
    check(rendered.text.find(L"---") == std::wstring::npos, "horizontal rule is converted");
    check(rendered.text.find(L"・ 不審なURL:") != std::wstring::npos, "unordered list uses Japanese bullet");
    check(rendered.text.find(L"1. URLをクリックしない") != std::wstring::npos, "ordered list numbering is retained");
    check(rendered.text.find(L"ogata 様") != std::wstring::npos, "inline code content is retained");
    check(!rendered.bold.empty(), "bold ranges are produced");
    check(!rendered.headings.empty(), "heading ranges are produced");
    check(!rendered.code.empty(), "inline code ranges are produced");
    const auto ordinary = MarkdownRenderer::Render(L"説明の前半です。\r\n------\r\n説明の後半です。");
    check(ordinary.text.find(L"説明の後半です。") != std::wstring::npos, "text around horizontal rules is retained");
    const auto plain = MarkdownRenderer::Render(L"通常の文章です。\r\n次の行です。");
    check(plain.text.find(L"通常の文章です。") != std::wstring::npos &&
        plain.text.find(L"次の行です。") != std::wstring::npos, "plain text and line breaks are retained");

    const auto codeMarker = MarkdownRenderer::Render(L"**重要な説明 `**` の続き**");
    check(codeMarker.text == L"重要な説明 ** の続き", "bold markers inside inline code are preserved");
    check(codeMarker.bold.size() == 1, "bold span closes after inline code");
    check(codeMarker.code.size() == 1, "inline code span is formatted independently");

    const auto unmatchedBold = MarkdownRenderer::Render(L"通常の文 **閉じていない太字");
    check(unmatchedBold.text == L"通常の文 **閉じていない太字", "unmatched bold marker remains literal");
    check(unmatchedBold.bold.empty(), "unmatched bold marker does not format trailing text");

    const auto unmatchedCode = MarkdownRenderer::Render(L"通常の文 `閉じていないコード");
    check(unmatchedCode.text == L"通常の文 `閉じていないコード", "unmatched code marker remains literal");
    check(unmatchedCode.code.empty(), "unmatched code marker does not format trailing text");

    const auto afterLines = MarkdownRenderer::Render(
        L"1行目\r\n2行目 **重要な単語** の後\r\n3行目 **最後**");
    check(afterLines.text.find(L"\r\n") == std::wstring::npos,
        "RichEdit output uses single-character paragraph breaks");
    check(afterLines.bold.size() == 2, "bold ranges after paragraph breaks are retained");
    if (afterLines.bold.size() == 2) {
        check(afterLines.text.substr(afterLines.bold[0].start, afterLines.bold[0].length) == L"重要な単語",
            "first bold range after a paragraph break aligns with the complete word");
        check(afterLines.text.substr(afterLines.bold[1].start, afterLines.bold[1].length) == L"最後",
            "later bold range remains aligned after multiple paragraph breaks");
    }

    const auto lfOnly = MarkdownRenderer::Render(
        L"1行目\n2行目 **重要な単語**\n3行目 **最後**");
    check(lfOnly.text.find(L"\n") == std::wstring::npos,
        "LF-only input is normalized to RichEdit paragraph breaks");
    check(lfOnly.text.find(L"\r\r") == std::wstring::npos,
        "LF-only input does not create overlapping empty paragraphs");
    check(lfOnly.bold.size() == 2,
        "bold ranges remain aligned after LF-only paragraph breaks");
    if (lfOnly.bold.size() == 2) {
        check(lfOnly.text.substr(lfOnly.bold[0].start, lfOnly.bold[0].length) == L"重要な単語",
            "first LF-only bold range is aligned");
        check(lfOnly.text.substr(lfOnly.bold[1].start, lfOnly.bold[1].length) == L"最後",
            "second LF-only bold range is aligned");
    }

    return failures == 0 ? 0 : 1;
}
