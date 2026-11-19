#pragma once
#include <string>

// Cloudflare's model-search records contain both a record identifier ("id")
// and the callable model identifier ("name"). The latter is typically
// "@cf/author/model"; numeric record IDs must never be used as model IDs.
inline std::string SelectWorkersAIModelIdentifier(
    const std::string& name, const std::string& id)
{
    if (!name.empty() && (name.rfind("@cf/", 0) == 0 || name.find('/') != std::string::npos))
        return name;
    if (!id.empty() && (id.rfind("@cf/", 0) == 0 || id.find('/') != std::string::npos))
        return id;
    return {};
}
