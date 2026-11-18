#pragma once

#include <string>

namespace GeminiReplyPromptContent
{
inline std::wstring BuildReplyRequirementsSection(const std::wstring& replyRequirements)
{
    if (replyRequirements.empty()) return {};
    return
        L"\n\n以下はユーザーが返信に必ず盛り込むよう指定した内容です。各項目を漏れなく反映してください。\n"
        L"指定内容が元メールと矛盾する場合は、事実として断定せず確認が必要な点として扱ってください。\n"
        L"【返信に必ず盛り込む内容（ユーザー指定）】\n" + replyRequirements;
}
}
