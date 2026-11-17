#pragma once

#include "MailSignatureStripper.h"
#include <string>

namespace MailPromptContent
{
inline std::wstring BuildEmailContext(
    const std::wstring& subject,
    const std::wstring& body,
    const std::wstring& subjectLabel = L"【件名】",
    const std::wstring& bodyLabel = L"【メール本文】")
{
    return subjectLabel + L"\n" + subject + L"\n\n" +
        bodyLabel + L"\n" + MailSignature::RemoveObviousSignature(body);
}
}
