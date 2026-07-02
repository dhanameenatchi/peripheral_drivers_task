#pragma once
// =============================================================================
// zephyr_i2c_mock.hpp — I2C host-side unit-test stubs
// =============================================================================
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>

#ifndef EIO
#define EIO 5
#endif

struct i2c_msg {
    uint8_t* buf;
    uint32_t len;
    uint8_t flags;
};

#define I2C_MSG_WRITE 0x00
#define I2C_MSG_READ  0x01
#define I2C_MSG_STOP  0x02

struct I2cTransaction {
    uint8_t address;
    uint8_t reg;
    bool is_write; // true if write, false if read
    size_t length;
};

namespace i2c_sim {
    // Per-address register banks: addr -> reg -> value
    inline uint8_t regs[128][256] = {};
    inline bool nack_next = false;  // inject NACK error on next I2C op
    // nack_after_n: when > 0, decrement on each I2C op; NACK when it reaches 0
    inline int nack_after_n = -1;   // -1 = disabled

    inline std::vector<I2cTransaction> transaction_history;
}

inline int i2c_write_read_dt(const void* dev, const uint8_t* wbuf, size_t wlen,
                             uint8_t* rbuf, size_t rlen)
{
    uint8_t addr = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(dev) & 0x7F);
    uint8_t reg = (wlen >= 1) ? wbuf[0] : static_cast<uint8_t>(0);

    i2c_sim::transaction_history.push_back({
        .address = addr,
        .reg = reg,
        .is_write = false,
        .length = rlen
    });

    if (i2c_sim::nack_next) {
        i2c_sim::nack_next = false;
        return -EIO;
    }
    if (i2c_sim::nack_after_n >= 0) {
        if (i2c_sim::nack_after_n == 0) {
            i2c_sim::nack_after_n = -1;
            return -EIO;
        }
        --i2c_sim::nack_after_n;
    }
    if (wlen >= 1) {
        for (size_t i = 0; i < rlen; ++i) {
            rbuf[i] = i2c_sim::regs[addr][(reg + i) & 0xFF];
        }
    }
    return 0;
}

inline uint8_t mock_shtc3_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

inline int i2c_read_dt(const void* dev, uint8_t* buf, size_t len) {
    uint8_t addr = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(dev) & 0x7F);

    i2c_sim::transaction_history.push_back({
        .address = addr,
        .reg = 0,
        .is_write = false,
        .length = len
    });

    if (i2c_sim::nack_next) {
        i2c_sim::nack_next = false;
        return -EIO;
    }
    if (i2c_sim::nack_after_n >= 0) {
        if (i2c_sim::nack_after_n == 0) {
            i2c_sim::nack_after_n = -1;
            return -EIO;
        }
        --i2c_sim::nack_after_n;
    }

    if (addr == 0x70 && len == 6) {
        buf[0] = i2c_sim::regs[0x70][0x00];
        buf[1] = i2c_sim::regs[0x70][0x01];
        buf[2] = mock_shtc3_crc8(buf, 2);
        buf[3] = i2c_sim::regs[0x70][0x02];
        buf[4] = i2c_sim::regs[0x70][0x03];
        buf[5] = mock_shtc3_crc8(buf + 3, 2);
    } else {
        for (size_t i = 0; i < len; ++i) {
            buf[i] = i2c_sim::regs[addr][i & 0xFF];
        }
    }
    return 0;
}


inline int i2c_write_dt(const void* dev, const uint8_t* buf, size_t len) {
    uint8_t addr = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(dev) & 0x7F);
    uint8_t reg = (len >= 1) ? buf[0] : static_cast<uint8_t>(0);

    i2c_sim::transaction_history.push_back({
        .address = addr,
        .reg = reg,
        .is_write = true,
        .length = (len >= 1) ? len - 1 : 0
    });

    if (i2c_sim::nack_next) {
        i2c_sim::nack_next = false;
        return -EIO;
    }
    if (len >= 2) {
        i2c_sim::regs[addr][buf[0]] = buf[1];
    }
    return 0;
}
