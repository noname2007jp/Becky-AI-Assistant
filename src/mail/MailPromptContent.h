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
    // Signature detection is heuristic. If it classifies the entire body
    // as a signature (possible when HTML text is flattened into one line),
    // preserve the original body rather than sending an empty message to Gemini.
    const std::wstring strippedBody = MailSignature::RemoveObviousSignature(body);
    const std::wstring& effectiveBody = strippedBody.empty() ? body : strippedBody;

    return subjectLabel + L"\n" + subject + L"\n\n" +
        bodyLabel + L"\n" + effectiveBody;
}
}
