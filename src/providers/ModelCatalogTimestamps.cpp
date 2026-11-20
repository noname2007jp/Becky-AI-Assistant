#include "ModelCatalogTimestamps.h"

#include <windows.h>
#include <shlobj.h>
#include <cwchar>

namespace
{
std::wstring AppDataFile(const wchar_t* fileName)
{
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK)
        return {};
    return std::wstring(appData) + L"\\Becky-AI-Assistant\\" + fileName;
}

std::wstring FormatLastWrite(const std::wstring& path, const wchar_t* label)
{
    if (path.empty()) return std::wstring(label) + L"未取得";
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes) ||
        (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return std::wstring(label) + L"未取得";

    FILETIME localTime{};
    SYSTEMTIME time{};
    if (!FileTimeToLocalFileTime(&attributes.ftLastWriteTime, &localTime) ||
        !FileTimeToSystemTime(&localTime, &time))
        return std::wstring(label) + L"不明";

    wchar_t buffer[80] = {};
    swprintf_s(buffer, _countof(buffer), L"%ls%04u/%02u/%02u %02u:%02u",
        label, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
    return buffer;
}
}

std::wstring GetModelCatalogUpdatedAtText(int provider)
{
    const wchar_t* file = nullptr;
    switch (provider)
    {
    case 0: file = L"models-catalog.txt"; break;
    case 1: file = L"groq-models-catalog.txt"; break;
    case 2: file = L"workers-ai-models-catalog.txt"; break;
    default: return L"最終取得：不明";
    }
    return FormatLastWrite(AppDataFile(file), L"最終取得：");
}

std::wstring GetModelPolicyUpdatedAtText()
{
    return FormatLastWrite(AppDataFile(L"model-catalog-policy.json"), L"評価情報更新：");
}
