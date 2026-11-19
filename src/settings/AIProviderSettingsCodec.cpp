#include "AIProviderSettingsCodec.h"

#include <limits>

namespace
{
constexpr std::uint32_t kMagic = 0x50494142; // "BAIP"
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kMaxStringChars = 4096;

void PutU32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    for (unsigned int i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xff));
}

bool GetU32(const std::uint8_t* data, std::size_t size, std::size_t& offset,
            std::uint32_t& value)
{
    if (offset > size || size - offset < 4) return false;
    value = 0;
    for (unsigned int i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(data[offset++]) << (i * 8);
    return true;
}

bool PutString(std::vector<std::uint8_t>& out, const std::wstring& value)
{
    if (value.size() > kMaxStringChars ||
        value.size() > std::numeric_limits<std::uint32_t>::max()) return false;
    PutU32(out, static_cast<std::uint32_t>(value.size()));
    const auto* p = reinterpret_cast<const std::uint8_t*>(value.data());
    out.insert(out.end(), p, p + value.size() * sizeof(wchar_t));
    return true;
}

bool GetString(const std::uint8_t* data, std::size_t size, std::size_t& offset,
               std::wstring& value)
{
    std::uint32_t chars = 0;
    if (!GetU32(data, size, offset, chars) || chars > kMaxStringChars) return false;
    const std::size_t bytes = static_cast<std::size_t>(chars) * sizeof(wchar_t);
    if (offset > size || size - offset < bytes || bytes % sizeof(wchar_t) != 0) return false;
    value.assign(reinterpret_cast<const wchar_t*>(data + offset), chars);
    offset += bytes;
    return true;
}
}

bool AIProviderSettingsCodec::Encode(const AIProviderSettings& s,
                                     std::vector<std::uint8_t>& bytes)
{
    bytes.clear();
    if (static_cast<unsigned int>(s.provider) > static_cast<unsigned int>(AIProvider::WorkersAI) ||
        static_cast<unsigned int>(s.route) > static_cast<unsigned int>(AIConnectionRoute::CloudflareAIGateway))
        return false;

    PutU32(bytes, kMagic);
    PutU32(bytes, kVersion);
    PutU32(bytes, static_cast<std::uint32_t>(s.provider));
    PutU32(bytes, static_cast<std::uint32_t>(s.route));
    PutU32(bytes, s.gatewayRequestLogging ? 1u : 0u);
    PutU32(bytes, s.gatewayCacheEnabled ? 1u : 0u);
    return PutString(bytes, s.geminiApiKey) &&
        PutString(bytes, s.geminiModel) &&
        PutString(bytes, s.groqApiKey) &&
        PutString(bytes, s.groqModel) &&
        PutString(bytes, s.workersAccountId) &&
        PutString(bytes, s.workersApiToken) &&
        PutString(bytes, s.workersModel) &&
        PutString(bytes, s.cloudflareAccountId) &&
        PutString(bytes, s.cloudflareGatewayId) &&
        PutString(bytes, s.cloudflareApiToken);
}

bool AIProviderSettingsCodec::Decode(const std::uint8_t* data, std::size_t size,
                                     AIProviderSettings& settings)
{
    if (!data || size < 24 || size > 1024 * 1024) return false;
    std::size_t offset = 0;
    std::uint32_t magic = 0, version = 0, provider = 0, route = 0, logging = 0, cache = 0;
    if (!GetU32(data, size, offset, magic) || magic != kMagic ||
        !GetU32(data, size, offset, version) || version != kVersion ||
        !GetU32(data, size, offset, provider) || provider > static_cast<std::uint32_t>(AIProvider::WorkersAI) ||
        !GetU32(data, size, offset, route) || route > static_cast<std::uint32_t>(AIConnectionRoute::CloudflareAIGateway) ||
        !GetU32(data, size, offset, logging) || logging > 1 ||
        !GetU32(data, size, offset, cache) || cache > 1)
        return false;

    AIProviderSettings decoded;
    decoded.provider = static_cast<AIProvider>(provider);
    decoded.route = static_cast<AIConnectionRoute>(route);
    decoded.gatewayRequestLogging = logging != 0;
    decoded.gatewayCacheEnabled = cache != 0;
    if (!GetString(data, size, offset, decoded.geminiApiKey) ||
        !GetString(data, size, offset, decoded.geminiModel) ||
        !GetString(data, size, offset, decoded.groqApiKey) ||
        !GetString(data, size, offset, decoded.groqModel) ||
        !GetString(data, size, offset, decoded.workersAccountId) ||
        !GetString(data, size, offset, decoded.workersApiToken) ||
        !GetString(data, size, offset, decoded.workersModel) ||
        !GetString(data, size, offset, decoded.cloudflareAccountId) ||
        !GetString(data, size, offset, decoded.cloudflareGatewayId) ||
        !GetString(data, size, offset, decoded.cloudflareApiToken) ||
        offset != size)
        return false;
    settings = std::move(decoded);
    return true;
}
