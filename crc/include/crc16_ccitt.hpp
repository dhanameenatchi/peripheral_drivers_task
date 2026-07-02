#pragma once
// =============================================================================
// crc16_ccitt.hpp — Crc16Ccitt strategy declaration
// =============================================================================
#include "crc_strategy.hpp"

class Crc16Ccitt final : public ICrcStrategy {
public:
    static constexpr uint16_t POLY = 0x1021;
    static constexpr uint16_t INIT = 0xFFFF;

    uint16_t compute(std::span<const uint8_t> data) const override;
    size_t      crcSize() const override;
    const char* name()    const override;
};
