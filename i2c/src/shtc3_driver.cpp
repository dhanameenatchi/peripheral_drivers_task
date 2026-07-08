#include "shtc3_driver.hpp"
#include <cmath>
#include <zephyr/logging/log.h>

#ifdef ZEPHYR_BUILD
LOG_MODULE_REGISTER(shtc3_driver, LOG_LEVEL_INF);
#else
#include <zephyr/logging/log.h>
#endif

#ifdef ZEPHYR_BUILD
#include <zephyr/kernel.h>
#define SLEEP_MS(ms) k_msleep(ms)
#else
#include <thread>
#include <chrono>
#define SLEEP_MS(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#endif

SHTC3Driver::SHTC3Driver(uint8_t addr)
    : bus_(addr)
{
    if (!bus_.writeCmd(CMD_WAKEUP)) {
        LOG_ERR("Failed to wake SHTC3");
        return;
    }

    SLEEP_MS(1);

    if (!bus_.writeCmd(CMD_SOFT_RESET)) {
        LOG_ERR("Failed to reset SHTC3");
        return;
    }

    SLEEP_MS(1);

    bus_.writeCmd(CMD_SLEEP);

    LOG_INF("SHTC3 initialized");
}

uint8_t SHTC3Driver::calculateCRC(const uint8_t* data, size_t len) {
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

int32_t SHTC3Driver::readRaw() {
    if (!bus_.writeCmd(CMD_WAKEUP)) {
    LOG_ERR("Wake command failed");
    raw_ = INT32_MIN;
    return INT32_MIN;
}
    SLEEP_MS(1);

    if (!bus_.writeCmd(CMD_MEAS_T_FIRST_POLL)) {
            LOG_ERR("Measurement command failed");
        bus_.writeCmd(CMD_SLEEP);
        raw_ = INT32_MIN;
        return INT32_MIN;
    }

    // Wait for conversion (normal mode, 12.1ms typ, 15ms safe)
    SLEEP_MS(15);

    uint8_t buf[6] = {};
    if (!bus_.readBytes(buf, 6)) {
        LOG_ERR("I2C read failed");
        bus_.writeCmd(CMD_SLEEP);
        raw_ = INT32_MIN;
        return INT32_MIN;
    }

    bus_.writeCmd(CMD_SLEEP);

    if (calculateCRC(buf, 2) != buf[2]) {
        LOG_ERR("Temperature CRC failed");
        raw_ = INT32_MIN;
        return INT32_MIN;
    }

    raw_ = (static_cast<int32_t>(buf[0]) << 8) | buf[1];
    return raw_;
}

float SHTC3Driver::toSI() {
    if (raw_ == INT32_MIN) return NAN;
    return -45.0f + 175.0f * static_cast<float>(raw_) / 65536.0f;
}

uint8_t SHTC3Driver::deviceId() {
    return bus_.address();
}

float SHTC3Driver::readTemperature_C() {
    if (readRaw() == INT32_MIN)
    return NAN;

return toSI();
}

float SHTC3Driver::readHumidity_pct() {
    if (!bus_.writeCmd(CMD_WAKEUP)) {
        LOG_ERR("Wake command failed");
        return NAN;
    }
    SLEEP_MS(1);

    if (!bus_.writeCmd(CMD_MEAS_T_FIRST_POLL)) {
        LOG_ERR("Measurement command failed");
        bus_.writeCmd(CMD_SLEEP);
        return NAN;
    }

    SLEEP_MS(15);

    uint8_t buf[6] = {};
    if (!bus_.readBytes(buf, 6)) {
        LOG_ERR("I2C read failed");
        bus_.writeCmd(CMD_SLEEP);
        return NAN;
    }

    bus_.writeCmd(CMD_SLEEP);

    if (calculateCRC(buf + 3, 2) != buf[5]) {
        LOG_ERR("Humidity CRC failed");
        return NAN;
    }

    int32_t raw_h = (static_cast<int32_t>(buf[3]) << 8) | buf[4];
    return 100.0f * static_cast<float>(raw_h) / 65536.0f;
}
