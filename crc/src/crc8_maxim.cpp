// =============================================================================
// crc8_maxim.cpp — Crc8Maxim strategy implementation
// =============================================================================
#include "crc8_maxim.hpp"

uint16_t Crc8Maxim::compute(std::span<const uint8_t> data) const {
    uint8_t crc = INIT;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x01) ? (crc >> 1) ^ POLY : (crc >> 1);
        }
    }
    return static_cast<uint16_t>(crc);
}

size_t Crc8Maxim::crcSize() const {
    return 1;
}

const char* Crc8Maxim::name() const {
    return "CRC8-Maxim";
}
