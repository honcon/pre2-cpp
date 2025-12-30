#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace sqz {

// Unpack an SQZ file, returns decompressed data
std::vector<uint8_t> unpack(const std::string& filename);

} // namespace sqz
