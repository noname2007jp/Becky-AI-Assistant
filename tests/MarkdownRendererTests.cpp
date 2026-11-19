#include "ui/MarkdownRenderer.h"
#include <iostream>
#include <string>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\\n";
        if (!ok) ++failures;
    };
    const std::wstring markdown =
        L"### 主な不審点・根拠\\r\\n"
        L"* **不審なURL:** 公式URLとは異なります。\\r\\n"
        L"* **宛名:** `ogata 様` となっています。\\r\\n"
        L"---\\r\\n"
        L"1. URLをクリックしない\\r\\n"
        L"2. 個人情報を入力しない\\r\\n";
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
    const auto ordinary = MarkdownRenderer::Render(L"説明の前半です。\\r\\n------\\r\\n説明の後半です。");
    check(ordinary.text.find(L"説明の後半です。") != std::wstring::npos, "text around horizontal rules is retained");
    const auto plain = MarkdownRenderer::Render(L"通常の文章です。\\r\\n次の行です。");
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

    return failures == 0 ? 0 : 1;
}
