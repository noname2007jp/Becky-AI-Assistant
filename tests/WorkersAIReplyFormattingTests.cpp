#include "WorkersAIReplyFormatting.h"

#include <cassert>
#include <string>

int main()
{
    using WorkersAIReplyFormatting::EnsureReadableLineBreaks;

    assert(EnsureReadableLineBreaks(L"お世話になります。ご連絡ありがとうございます。よろしくお願いいたします。") ==
        L"お世話になります。\r\nご連絡ありがとうございます。\r\nよろしくお願いいたします。");

    assert(EnsureReadableLineBreaks(L"お世話になります。\r\nご確認ください。") ==
        L"お世話になります。\r\nご確認ください。");

    assert(EnsureReadableLineBreaks(L"第一段落です。第二文です。\n第三文です。") ==
        L"第一段落です。\r\n第二文です。\r\n第三文です。");

    assert(EnsureReadableLineBreaks(L"ご確認ください。」続けてご案内します。") ==
        L"ご確認ください。」\r\n続けてご案内します。");

    assert(EnsureReadableLineBreaks(L"箇条書きです\r\n・項目A\r\n・項目B") ==
        L"箇条書きです\r\n・項目A\r\n・項目B");

    assert(EnsureReadableLineBreaks(L"改行なし") == L"改行なし");
    return 0;
}
