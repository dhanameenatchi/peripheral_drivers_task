// =============================================================================
// crc16_ccitt.cpp — Crc16Ccitt strategy implementation
// =============================================================================
#include "crc16_ccitt.hpp"

uint16_t Crc16Ccitt::compute(std::span<const uint8_t> data) const {
    uint16_t crc = INIT;
    for (uint8_t byte : data) {
        crc ^= static_cast<uint16_t>(byte) << 8;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 0x8000) ? (crc << 1) ^ POLY : (crc << 1);
        }
    }
    return crc;
}

size_t Crc16Ccitt::crcSize() const {
    return 2;
}

const char* Crc16Ccitt::name() const {
    return "CRC16-CCITT";
}
