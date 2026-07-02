#pragma once
// =============================================================================
// crc8_maxim.hpp — Crc8Maxim strategy declaration
// =============================================================================
#include "crc_strategy.hpp"

class Crc8Maxim final : public ICrcStrategy {
public:
    static constexpr uint8_t POLY = 0x31;
    static constexpr uint8_t INIT = 0x00;

    uint16_t compute(std::span<const uint8_t> data) const override;
    size_t      crcSize() const override;
    const char* name()    const override;
};
