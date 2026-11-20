#pragma once

#include <string>

// provider: 0=Gemini, 1=Groq, 2=Cloudflare Workers AI.
// Returns the local timestamp of the last successfully saved provider catalog.
std::wstring GetModelCatalogUpdatedAtText(int provider);

// Returns the local timestamp of the cached model quality policy.
std::wstring GetModelPolicyUpdatedAtText();
