#include "providers/ModelCatalogPolicy.h"

#include <cassert>
#include <string>
#include <vector>

int main()
{
    const std::string valid = R"json({
      "schema_version": 1,
      "providers": {
        "gemini": [],
        "groq": [{
          "id": "qwen/qwen3.8-27b",
          "status": "recommended",
          "quality": "tested",
          "reason": "tested"
        }],
        "workers_ai": [{
          "id": "@cf/meta/llama-3.3-70b-instruct-fp8-fast",
          "status": "caution",
          "quality": "unverified",
          "reason": "check before use"
        }]
      }
    })json";
    std::vector<ModelCatalogPolicyEntry> entries;
    assert(ParseModelCatalogPolicyJson(valid, entries));
    assert(entries.size() == 2);
    assert(entries[0].provider == 1);
    assert(entries[0].modelId == L"qwen/qwen3.8-27b");
    assert(entries[0].status == L"recommended");
    assert(entries[1].provider == 2);
    assert(entries[1].quality == L"unverified");

    const std::string invalidStatus = R"json({
      "schema_version": 1,
      "providers": {
        "gemini": [],
        "groq": [{"id":"x","status":"best","quality":"tested"}],
        "workers_ai": []
      }
    })json";
    assert(!ParseModelCatalogPolicyJson(invalidStatus, entries));

    const std::string duplicate = R"json({
      "schema_version": 1,
      "providers": {
        "gemini": [],
        "groq": [
          {"id":"x","status":"allowed","quality":"tested"},
          {"id":"x","status":"caution","quality":"poor"}
        ],
        "workers_ai": []
      }
    })json";
    assert(!ParseModelCatalogPolicyJson(duplicate, entries));
    return 0;
}
