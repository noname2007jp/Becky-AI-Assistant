#pragma once

#include "AIProviderSettings.h"
#include <cstddef>
#include <cstdint>
#include <vector>

// Internal binary codec kept separate from DPAPI so format validation is testable.
namespace AIProviderSettingsCodec
{
bool Encode(const AIProviderSettings& settings, std::vector<std::uint8_t>& bytes);
bool Decode(const std::uint8_t* data, size_t size, AIProviderSettings& settings);
}
