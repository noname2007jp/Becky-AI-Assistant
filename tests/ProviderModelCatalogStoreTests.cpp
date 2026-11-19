#include "providers/ProviderModelCatalogStore.h"
#include <string>
#include <vector>

int main()
{
    const std::vector<std::wstring> groq = {
        L"llama-3.3-70b-versatile", L"openai/gpt-oss-20b"
    };
    std::wstring error;
    if (!SaveProviderModelCatalog(1, groq, error)) return 1;
    std::vector<std::wstring> loaded;
    if (!LoadProviderModelCatalog(1, loaded, error) || loaded != groq) return 2;

    const std::vector<std::wstring> workers = {
        L"@cf/meta/llama-3.1-8b-instruct", L"@cf/qwen/qwen2.5-7b-instruct"
    };
    if (!SaveProviderModelCatalog(2, workers, error)) return 3;
    loaded.clear();
    if (!LoadProviderModelCatalog(2, loaded, error) || loaded != workers) return 4;

    // Invalid provider IDs and empty catalogs must not overwrite a valid cache.
    if (SaveProviderModelCatalog(3, workers, error)) return 5;
    if (SaveProviderModelCatalog(2, {}, error)) return 6;
    loaded.clear();
    if (!LoadProviderModelCatalog(2, loaded, error) || loaded != workers) return 7;
    return 0;
}
