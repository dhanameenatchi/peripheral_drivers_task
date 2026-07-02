#include "pav3015_driver.hpp"
#include <cmath>
#ifdef ZEPHYR_BUILD
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pav3015_driver, LOG_LEVEL_INF);
#endif
PAV3015Driver::PAV3015Driver(uint8_t addr, uint8_t range)
    : bus_(addr), range_(range)
{
    LOG_INF("PAV3015 initialized");

    if (!setRange(range))
    {
        LOG_ERR("Failed to initialize PAV3015");
    }
}

bool PAV3015Driver::setRange(uint8_t range)
{
    range_ = range;
    return true;
}

int32_t PAV3015Driver::readRaw()
{
    uint8_t buf[5] = {};

    // Read 5 bytes directly (NO register address)
    if (!bus_.readBytes(buf, 5))
    {
        LOG_ERR("PAV3015 read failed");
        raw_valid_ = false;
        raw_ = INT32_MIN;
        return INT32_MIN;
    }

    LOG_INF("Frame = %02X %02X %02X %02X %02X",
        buf[0],
        buf[1],
        buf[2],
        buf[3],
        buf[4]);

if (!validateChecksum(buf, 5))
{
    LOG_ERR("Checksum failed");
    raw_valid_ = false;
    raw_ = INT32_MIN;
    return INT32_MIN;
}
    raw_ = (static_cast<uint16_t>(buf[1]) << 8) | buf[2];
    LOG_INF("ADC = %u", raw_);

    raw_valid_ = true;

    return raw_;
}
// Computes velocity from cached raw value. This avoids duplicate I2C transactions
// when both velocity and raw code are requested in the same sampling loop.
float PAV3015Driver::toSI() {
    if (!raw_valid_) {
        readRaw();
    }
    raw_valid_ = false; // consume the cached reading
    if (raw_ == INT32_MIN) return NAN;
    uint16_t adc = (static_cast<uint16_t>(buf[0] & 0x0F) << 8) |
                static_cast<uint16_t>(buf[1]);
    return (range_ == RANGE_15MPS) ? interpolate15(adc) : interpolate7(adc);
}

uint8_t PAV3015Driver::deviceId() {
    return bus_.address();
}

float PAV3015Driver::readVelocity_mps() {
    readRaw();
    return toSI();
}

bool PAV3015Driver::validateChecksum(const uint8_t *buf, size_t len)
{
    if (len != 5)
    {
        return false;
    }

    uint16_t sum = 0;

    for (size_t i = 0; i < 5; ++i)
    {
        sum += buf[i];
    }

    return ((sum & 0xFF) == 0);
}
