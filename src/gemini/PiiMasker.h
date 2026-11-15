#pragma once
#include <string>
#include <utility>
#include <vector>

class PiiMasker
{
public:
    PiiMasker();
    std::wstring Mask(const std::wstring& value);
    std::wstring Restore(const std::wstring& value) const;

private:
    std::vector<std::pair<std::wstring, std::wstring>> replacements_;
    unsigned int nextId_ = 1;
    std::wstring AddReplacement(const std::wstring& value, const wchar_t* label);
    std::wstring ApplyPattern(const std::wstring& value, const std::wstring& pattern,
                              const wchar_t* label);
};
