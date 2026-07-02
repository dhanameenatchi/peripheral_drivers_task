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
// Shared GoogleTest fixture I2CTest
// ---------------------------------------------------------------------------
class I2CTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::memset(i2c_sim::regs, 0, sizeof(i2c_sim::regs));
        i2c_sim::nack_next = false;
        i2c_sim::nack_after_n = -1;
        i2c_sim::transaction_history.clear();
    }
};
