#include "providers/ProviderModelCatalogStore.h"
#include <cassert>
#include <string>
#include <vector>

int main()
{
    const std::vector<std::wstring> groq = {
        L"llama-3.3-70b-versatile", L"openai/gpt-oss-20b"
    };
    std::wstring error;
    assert(SaveProviderModelCatalog(1, groq, error));
    std::vector<std::wstring> loaded;
    assert(LoadProviderModelCatalog(1, loaded, error));
    assert(loaded == groq);

    const std::vector<std::wstring> workers = {
        L"@cf/meta/llama-3.1-8b-instruct", L"@cf/qwen/qwen2.5-7b-instruct"
    };
    assert(SaveProviderModelCatalog(2, workers, error));
    loaded.clear();
    assert(LoadProviderModelCatalog(2, loaded, error));
    assert(loaded == workers);

    assert(!SaveProviderModelCatalog(3, workers, error));
    assert(!SaveProviderModelCatalog(2, {}, error));
    loaded.clear();
    assert(LoadProviderModelCatalog(2, loaded, error));
    assert(loaded == workers);
    return 0;
}
