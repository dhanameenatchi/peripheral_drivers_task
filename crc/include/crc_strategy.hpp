#pragma once
// =============================================================================
// crc_strategy.hpp — ICrcStrategy interface declaration
// =============================================================================
#include <cstdint>
#include <span>

struct ICrcStrategy {
    virtual ~ICrcStrategy() = default;
    virtual uint16_t compute(std::span<const uint8_t> data) const = 0;
    virtual size_t   crcSize() const = 0;  // 1 or 2 bytes
    virtual const char* name() const = 0;
};
