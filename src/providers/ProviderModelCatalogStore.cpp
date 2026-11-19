#include "ProviderModelCatalogStore.h"

#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace
{
bool GetCatalogPath(int provider, std::wstring& path, bool createDirectory)
{
    if (provider != 1 && provider != 2) return false;
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA | (createDirectory ? CSIDL_FLAG_CREATE : 0),
        nullptr, SHGFP_TYPE_CURRENT, appData) != S_OK) return false;
    const std::wstring directory = std::wstring(appData) + L"\\Becky-AI-Assistant";
    if (createDirectory && !CreateDirectoryW(directory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) return false;
    path = directory + (provider == 1 ? L"\\groq-models-catalog.txt" : L"\\workers-ai-models-catalog.txt");
    return true;
}
bool ToUtf8(const std::wstring& value, std::string& output)
{
    if (value.empty()) { output.clear(); return true; }
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return false;
    output.resize(static_cast<size_t>(size));
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), output.data(), size, nullptr, nullptr) == size;
}
bool FromUtf8(const std::string& value, std::wstring& output)
{
    if (value.empty()) { output.clear(); return true; }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return false;
    output.resize(static_cast<size_t>(size));
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), output.data(), size) == size;
}
}

bool LoadProviderModelCatalog(int provider, std::vector<std::wstring>& modelIds,
    std::wstring& error)
{
    modelIds.clear();
    error.clear();
    std::wstring path;
    if (!GetCatalogPath(provider, path, false)) {
        error = L"モデル一覧の保存先を取得できません。";
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) return false; // A missing cache is normal on first launch.
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.size() > 1024 * 1024 || bytes.rfind("BAIMC1\n", 0) != 0) {
        error = L"保存済みモデル一覧の形式が正しくありません。";
        return false;
    }
    size_t start = 7;
    while (start < bytes.size()) {
        size_t end = bytes.find('\n', start);
        if (end == std::string::npos) end = bytes.size();
        std::string line = bytes.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) {
            std::wstring id;
            if (!FromUtf8(line, id) || id.size() > 4096) {
                modelIds.clear();
                error = L"保存済みモデル一覧に不正な文字列があります。";
                return false;
            }
            modelIds.push_back(std::move(id));
            if (modelIds.size() > 1000) {
                modelIds.clear();
                error = L"保存済みモデル一覧の件数が上限を超えています。";
                return false;
            }
        }
        start = end + 1;
    }
    std::sort(modelIds.begin(), modelIds.end());
    modelIds.erase(std::unique(modelIds.begin(), modelIds.end()), modelIds.end());
    return !modelIds.empty();
}

bool SaveProviderModelCatalog(int provider, const std::vector<std::wstring>& modelIds,
    std::wstring& error)
{
    error.clear();
    if (modelIds.empty() || modelIds.size() > 1000) {
        error = L"保存するモデル一覧の件数が正しくありません。";
        return false;
    }
    std::string bytes = "BAIMC1\n";
    for (const auto& id : modelIds) {
        if (id.empty() || id.size() > 4096 || id.find_first_of(L"\\r\\n") != std::wstring::npos) {
            error = L"モデルIDに保存できない文字が含まれています。";
            return false;
        }
        std::string line;
        if (!ToUtf8(id, line) || line.find_first_of("\\r\\n") != std::string::npos) {
            error = L"モデルIDをUTF-8に変換できません。";
            return false;
        }
        bytes += line;
        bytes += '\n';
        if (bytes.size() > 1024 * 1024) {
            error = L"モデル一覧のサイズが上限を超えています。";
            return false;
        }
    }
    std::wstring path;
    if (!GetCatalogPath(provider, path, true)) {
        error = L"モデル一覧の保存先を作成できません。";
        return false;
    }
    const std::wstring tempPath = path + L".tmp";
    std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
    const bool written = file && static_cast<bool>(file.write(bytes.data(), static_cast<std::streamsize>(bytes.size())));
    file.close();
    if (!written || !MoveFileExW(tempPath.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath.c_str());
        error = L"モデル一覧を保存できません。";
        return false;
    }
    return true;
}
