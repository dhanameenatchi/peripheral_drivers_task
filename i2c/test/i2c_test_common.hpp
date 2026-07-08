#pragma once
// =============================================================================
// i2c_test_common.hpp — Shared fixtures and helpers for I2C test files
// =============================================================================
#include <gtest/gtest.h>
#include "sensor_factory.hpp"
#include <cmath>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------
// Helper utilities to configure simulated registers
// ---------------------------------------------------------------------------
inline void setReg(uint8_t addr, uint8_t reg, uint8_t val) {
    i2c_sim::regs[addr][reg] = val;
}

inline void setReg16_BE(uint8_t addr, uint8_t reg, uint16_t val) {
    i2c_sim::regs[addr][reg]     = static_cast<uint8_t>(val >> 8);
    i2c_sim::regs[addr][reg + 1] = static_cast<uint8_t>(val & 0xFF);
}

inline void setReg24_LE(uint8_t addr, uint8_t reg, uint32_t val) {
    i2c_sim::regs[addr][reg]     = static_cast<uint8_t>(val & 0xFF);
    i2c_sim::regs[addr][reg + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    i2c_sim::regs[addr][reg + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
}

// ---------------------------------------------------------------------------
// Helper to set up default BME280 calibration data
// ---------------------------------------------------------------------------
inline void setupBME280Calibration(uint8_t addr) {
    // Chip ID
    i2c_sim::regs[addr][0xD0] = 0x60;

    // T1..P9 block (0x88..0x9F)
    // T1: 32768 (0x8000)
    i2c_sim::regs[addr][0x88] = 0x00;
    i2c_sim::regs[addr][0x89] = 0x80;
    // T2: 20000 (0x4E20)
    i2c_sim::regs[addr][0x8A] = 0x20;
    i2c_sim::regs[addr][0x8B] = 0x4E;
    // T3: 10 (0x000A)
    i2c_sim::regs[addr][0x8C] = 0x0A;
    i2c_sim::regs[addr][0x8D] = 0x00;

    // P1: 36477 (0x8E3D)
    i2c_sim::regs[addr][0x8E] = 0x3D;
    i2c_sim::regs[addr][0x8F] = 0x8E;
    // P2: -10685 (0xD5C3)
    i2c_sim::regs[addr][0x90] = 0xC3;
    i2c_sim::regs[addr][0x91] = 0xD5;
    // P3: 3024 (0x0BD0)
    i2c_sim::regs[addr][0x92] = 0xD0;
    i2c_sim::regs[addr][0x93] = 0x0B;
    // P4: 2855 (0x0B27)
    i2c_sim::regs[addr][0x94] = 0x27;
    i2c_sim::regs[addr][0x95] = 0x0B;
    // P5: 140 (0x008C)
    i2c_sim::regs[addr][0x96] = 0x8C;
    i2c_sim::regs[addr][0x97] = 0x00;
    // P6: -7 (0xFFF9)
    i2c_sim::regs[addr][0x98] = 0xF9;
    i2c_sim::regs[addr][0x99] = 0xFF;
    // P7: 15500 (0x3C8C)
    i2c_sim::regs[addr][0x9A] = 0x8C;
    i2c_sim::regs[addr][0x9B] = 0x3C;
    // P8: -14600 (0xC6F8)
    i2c_sim::regs[addr][0x9C] = 0xF8;
    i2c_sim::regs[addr][0x9D] = 0xC6;
    // P9: 6000 (0x1770)
    i2c_sim::regs[addr][0x9E] = 0x70;
    i2c_sim::regs[addr][0x9F] = 0x17;

    // H1: 75 (0x4B)
    i2c_sim::regs[addr][0xA1] = 0x4B;

    // H2..H6 block (0xE1..0xE7)
    i2c_sim::regs[addr][0xE1] = 0x6D;
    i2c_sim::regs[addr][0xE2] = 0x01;
    i2c_sim::regs[addr][0xE3] = 0x00;
    i2c_sim::regs[addr][0xE4] = 0x00;
    i2c_sim::regs[addr][0xE5] = 0x00;
    i2c_sim::regs[addr][0xE6] = 0x00;
    i2c_sim::regs[addr][0xE7] = 0x00;
}

// ---------------------------------------------------------------------------
// Shared GoogleTest fixture I2CTest
// ---------------------------------------------------------------------------
class I2CTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::memset(i2c_sim::regs, 0, sizeof(i2c_sim::regs));
        i2c_sim::nack_next = false;
        i2c_sim::nack_after_n = -1;
        i2c_sim::transaction_history.clear();

        // Default device WHO_AM_I and Chip ID mocks
        i2c_sim::regs[0x5C][0x0F] = 0xB1; // LPS22HB WHO_AM_I (0x5C)
        i2c_sim::regs[0x5D][0x0F] = 0xB1; // LPS22HB WHO_AM_I (0x5D)
        
        setupBME280Calibration(0x76);     // BME280 default calibration and Chip ID
        setupBME280Calibration(0x77);     // BME280 custom calibration and Chip ID (for custom factory test)
    }
};
