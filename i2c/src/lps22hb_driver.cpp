#include "lps22hb_driver.hpp"
#include <cmath>
#ifdef ZEPHYR_BUILD
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lps22hb_driver, LOG_LEVEL_INF);
#else
#include <zephyr/logging/log.h>
#endif

LPS22HbDriver::LPS22HbDriver(uint8_t addr, uint8_t odr)
    : bus_(addr)
{
    uint8_t id = 0;

    if (!bus_.readRegs(REG_WHO_AM_I, &id, 1))
    {
        LOG_ERR("LPS22HB WHO_AM_I read failed");
        return;
    }

    if (id != CHIP_ID)
    {
        LOG_ERR("Unexpected LPS22HB ID: 0x%02X", id);
        return;
    }

    LOG_INF("LPS22HB detected");

    configODR(odr);
}

void LPS22HbDriver::configODR(uint8_t odr)
{
    if (!bus_.writeReg(REG_CTRL1,
                       static_cast<uint8_t>((odr & 0x07) << 4)))
    {
        LOG_ERR("Failed to configure LPS22HB");
    }
    odr_ = odr;
}

int32_t LPS22HbDriver::readRaw()
{
    uint8_t buf[3] = {};
    if (!bus_.readRegs(REG_PRESS_XL, buf, 3))
    {
        LOG_ERR("Pressure read failed");
        raw_ = INT32_MIN;
        return INT32_MIN;
    }
    // 24-bit two's complement, little-endian
    raw_ = static_cast<int32_t>(
        static_cast<uint32_t>(buf[0]) |
        (static_cast<uint32_t>(buf[1]) << 8) |
        (static_cast<uint32_t>(buf[2]) << 16));
    if (raw_ & 0x800000)
        raw_ |= static_cast<int32_t>(0xFF000000); // sign-extend
    return raw_;
}

float LPS22HbDriver::toSI()
{
    if (raw_ == INT32_MIN)
        return NAN;
    return static_cast<float>(raw_) / 4096.0f * 100.0f; // hPa → Pa
}

uint8_t LPS22HbDriver::deviceId()
{
    return bus_.address();
}

float LPS22HbDriver::readPressure_hPa()
{
    readRaw();
    if (raw_ == INT32_MIN)
        return NAN;
    return static_cast<float>(raw_) / 4096.0f;
}

float LPS22HbDriver::readTemp_C()
{
    uint8_t buf[2] = {};
    if (!bus_.readRegs(REG_TEMP_L, buf, 2))
    {
        LOG_ERR("Temperature read failed");
        return NAN;
    }
    int16_t raw_t = static_cast<int16_t>(
        static_cast<uint16_t>(buf[0]) | (static_cast<uint16_t>(buf[1]) << 8));
    return static_cast<float>(raw_t) / 100.0f;
}

float LPS22HbDriver::readPressureFiltered_hPa()
{
    float p = readPressure_hPa();

    if (std::isnan(p))
        return NAN;
    avg_buf_[avg_idx_] = p;
    avg_idx_ = (avg_idx_ + 1) % AVG_TAPS;
    if (avg_count_ < AVG_TAPS)
        ++avg_count_;
    float sum = 0.0f;
    for (size_t i = 0; i < avg_count_; ++i)
        sum += avg_buf_[i];
    return sum / static_cast<float>(avg_count_);
}
