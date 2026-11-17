#include "PiiMasker.h"
#include "settings/ApiKeyStore.h"
#include <algorithm>
#include <regex>

PiiMasker::PiiMasker()
{
    if (!LoadPiiMaskOptions(options_, error_)) return;
    if (!LoadPiiMaskTerms(customTerms_, error_)) return;
    std::sort(customTerms_.begin(), customTerms_.end(),
        [](const std::wstring& a, const std::wstring& b) { return a.size() > b.size(); });
}

PiiMasker::PiiMasker(const PiiMaskOptions& options, std::vector<std::wstring> customTerms)
    : customTerms_(std::move(customTerms)), options_(options)
{
    std::sort(customTerms_.begin(), customTerms_.end(),
        [](const std::wstring& a, const std::wstring& b) { return a.size() > b.size(); });
}

std::wstring PiiMasker::AddReplacement(const std::wstring& value, const wchar_t* label)
{
    if (value.empty()) return value;
    for (const auto& item : replacements_)
        if (item.first == value) return item.second;
    const std::wstring token = L"[" + std::wstring(label) + L"_" +
        std::to_wstring(nextId_++) + L"]";
    replacements_.push_back({ value, token });
    return token;
}

std::wstring PiiMasker::ApplyPattern(const std::wstring& value,
    const std::wstring& pattern, const wchar_t* label)
{
    try
    {
        const std::wregex expression(pattern, std::regex_constants::ECMAScript |
            std::regex_constants::icase);
        std::wstring output;
        std::wsregex_iterator current(value.begin(), value.end(), expression), end;
        size_t copied = 0;
        for (; current != end; ++current)
        {
            const auto& match = *current;
            const size_t position = static_cast<size_t>(match.position());
            const size_t length = static_cast<size_t>(match.length());
            output.append(value, copied, position - copied);
            output += AddReplacement(match.str(), label);
            copied = position + length;
        }
        if (copied == 0) return value;
        output.append(value, copied, std::wstring::npos);
        return output;
    }
    catch (...) { return value; }
}

std::wstring PiiMasker::Mask(const std::wstring& value)
{
    if (!options_.enabled) return value;
    std::wstring output = value;
    // Apply manual selections first so complete previews can be replaced atomically.
    for (const auto& term : manualTerms_)
    {
        if (term.size() < 2) continue;
        size_t position = 0;
        while ((position = output.find(term, position)) != std::wstring::npos)
        {
            const std::wstring token = AddReplacement(term, L"MANUAL");
            output.replace(position, term.size(), token);
            position += token.size();
        }
    }
    if (options_.email) output = ApplyPattern(output,
        L"[A-Za-z0-9._%+\\-]+@[A-Za-z0-9.\\-]+\\.[A-Za-z]{2,}", L"EMAIL");
    if (options_.company) output = ApplyPattern(output,
        L"(?:株式会社|有限会社|合同会社|合資会社|合名会社)[一-龠々ぁ-んァ-ヶA-Za-z0-9・＆&（）()ー－]{1,24}|[一-龠々ぁ-んァ-ヶA-Za-z0-9・＆&（）()ー－]{1,24}(?:株式会社|有限会社|合同会社|合資会社|合名会社)",
        L"COMPANY");
    if (options_.person) output = ApplyPattern(output,
        L"(?:氏名|担当者名|ご担当者|お名前|名前|差出人|送信者|宛先|宛名|担当者)[ \\t]*[:：】][ \\t]*[一-龠々ぁ-んァ-ヶ]{2,12}(?:[ \\t]*[一-龠々ぁ-んァ-ヶ]{1,12})?",
        L"PERSON");
    if (options_.person) output = ApplyPattern(output,
        L"[一-龠々ぁ-んァ-ヶ]{2,8}(?:[ \\t]+[一-龠々ぁ-んァ-ヶ]{1,8})?(?:様|さん|氏)",
        L"PERSON");
    if (options_.address) output = ApplyPattern(output,
        L"(?:住所|所在地|送付先|Address)\\s*[:：]\\s*[^\\r\\n]+",
        L"ADDRESS");
    if (options_.account) output = ApplyPattern(output,
        L"(?:口座番号|カード番号|顧客番号|社員番号)\\s*[:：]\\s*[0-9０-９ -]{4,24}",
        L"ACCOUNT");
    if (options_.phone) output = ApplyPattern(output,
        L"0[0-9０-９]{1,4}[-ー－ ]?[0-9０-９]{1,4}[-ー－ ]?[0-9０-９]{3,4}",
        L"PHONE");
    if (options_.postal) output = ApplyPattern(output,
        L"[0-9０-９]{3}-?[0-9０-９]{4}", L"POSTAL");
    if (options_.ip) output = ApplyPattern(output,
        L"(?:[0-9]{1,3}\\.){3}[0-9]{1,3}", L"IP");
    if (options_.custom) for (const auto& term : customTerms_)
    {
        if (term.size() < 2) continue;
        size_t position = 0;
        while ((position = output.find(term, position)) != std::wstring::npos)
        {
            const std::wstring token = AddReplacement(term, L"CUSTOM");
            output.replace(position, term.size(), token);
            position += token.size();
        }
    }
    return output;
}

bool PiiMasker::MaskSelected(std::wstring& preview, size_t start, size_t length)
{
    if (!options_.enabled || length == 0 || start > preview.size() || length > preview.size() - start) return false;
    const std::wstring selected = preview.substr(start, length);
    if (selected.size() < 2 || selected.find(L"[MANUAL_") != std::wstring::npos) return false;
    const std::wstring originalSelected = Restore(selected);
    if (originalSelected.size() < 2 ||
        std::find(manualTerms_.begin(), manualTerms_.end(), originalSelected) != manualTerms_.end()) return false;
    manualTerms_.push_back(originalSelected);
    size_t position = 0;
    const std::wstring token = AddReplacement(originalSelected, L"MANUAL");
    while ((position = preview.find(selected, position)) != std::wstring::npos)
    {
        preview.replace(position, selected.size(), token);
        position += token.size();
    }
    return true;
}

bool PiiMasker::ContainsAllTokens(const std::wstring& value) const
{
    for (const auto& item : replacements_)
        if (value.find(item.second) == std::wstring::npos) return false;
    return true;
}

std::wstring PiiMasker::Restore(const std::wstring& value) const
{
    std::wstring output = value;
    for (const auto& item : replacements_)
    {
        size_t position = 0;
        while ((position = output.find(item.second, position)) != std::wstring::npos)
        {
            output.replace(position, item.second.size(), item.first);
            position += item.first.size();
        }
    }

    // Gemini may append a Japanese honorific to a PERSON placeholder even
    // though the original replacement already contains that honorific.
    // Collapse only immediately repeated honorifics after restoration.
    const std::pair<std::wstring, std::wstring> honorifics[] = {
        { L"様様", L"様" }, { L"さんさん", L"さん" }, { L"氏氏", L"氏" }
    };
    for (const auto& honorific : honorifics)
    {
        size_t position = 0;
        while ((position = output.find(honorific.first, position)) != std::wstring::npos)
        {
            output.replace(position, honorific.first.size(), honorific.second);
            position += honorific.second.size();
        }
    }
    return output;
}
