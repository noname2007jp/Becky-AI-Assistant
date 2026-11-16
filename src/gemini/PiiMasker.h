#pragma once
#include <string>
#include <utility>
#include <vector>
#include "settings/ApiKeyStore.h"

class PiiMasker
{
public:
    PiiMasker();
    std::wstring Mask(const std::wstring& value);
    bool MaskSelected(std::wstring& preview, size_t start, size_t length);
    std::wstring Restore(const std::wstring& value) const;
    bool IsReady() const { return error_.empty(); }
    const std::wstring& Error() const { return error_; }

private:
    std::vector<std::pair<std::wstring, std::wstring>> replacements_;
    unsigned int nextId_ = 1;
    std::vector<std::wstring> customTerms_;
    std::vector<std::wstring> manualTerms_;
    std::wstring error_;
    PiiMaskOptions options_;
    std::wstring AddReplacement(const std::wstring& value, const wchar_t* label);
    std::wstring ApplyPattern(const std::wstring& value, const std::wstring& pattern,
                              const wchar_t* label);
};
