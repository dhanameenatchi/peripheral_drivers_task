#include "pav3015_driver.hpp"
#include <cmath>
#include <zephyr/logging/log.h>

#ifdef ZEPHYR_BUILD
LOG_MODULE_REGISTER(pav3015_driver, LOG_LEVEL_INF);
#else
#include <zephyr/logging/log.h>
#endif

PAV3015Driver::PAV3015Driver(uint8_t addr, uint8_t range)
    : bus_(addr), range_(range)
{
    if (!setRange(range))
    {
        LOG_ERR("Failed to initialize PAV3015");
    }
}

bool PAV3015Driver::setRange(uint8_t range)
{
    if (range != RANGE_7MPS && range != RANGE_15MPS)
    {
        return false;
    }
    range_ = range;

    uint8_t buf[5] = {};
    if (!bus_.readBytes(buf, 5))
    {
        return false;
    }
    return validateChecksum(buf, 5);
}

int32_t PAV3015Driver::readRaw()
{
    uint8_t buf[5] = {};
    if (!bus_.readBytes(buf, 5)) {
        LOG_ERR("PAV3015 data read failed");
        raw_valid_ = false;
        raw_ = INT32_MIN;
        return INT32_MIN;
    }

    if (!validateChecksum(buf, 5)) {
        LOG_ERR("PAV3015 checksum validation failed");
        raw_valid_ = false;
        raw_ = INT32_MIN;
        return INT32_MIN;
    }

    raw_ = (static_cast<uint32_t>(buf[1]) << 8) | buf[2];
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
    uint16_t adc = static_cast<uint16_t>(raw_);
    return (range_ == RANGE_15MPS) ? interpolate15(adc) : interpolate7(adc);
}

uint8_t PAV3015Driver::deviceId() {
    return bus_.address();
}

float PAV3015Driver::readVelocity_mps() {
    if (readRaw() == INT32_MIN) {
        return NAN;
    }
    return toSI();
}

bool PAV3015Driver::validateChecksum(const uint8_t *buf, size_t len)
{
    if (len < 3)
    {
        return false;
    }

    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        sum += buf[i];
    }

    return sum == 0;
}
