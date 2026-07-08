#ifdef ZEPHYR_BUILD
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bme280_driver, LOG_LEVEL_INF);
#else
#include <zephyr/logging/log.h>
#endif

#include "bme280_driver.hpp"
#include <cmath>

BME280Driver::BME280Driver(uint8_t addr) : bus_(addr)
{
    uint8_t id = 0;

    if (!bus_.readRegs(REG_CHIP_ID, &id, 1))
    {
        LOG_ERR("Failed to read BME280 Chip ID");
        return;
    }

    if (id != CHIP_ID)
    {
        LOG_ERR("Unexpected BME280 Chip ID: 0x%02X", id);
        return;
    }

    LOG_INF("BME280 detected");

    // osrs_t=2, osrs_p=16x, normal mode
    if (!bus_.writeReg(REG_CTRL_MEAS, 0x57))
{
    LOG_ERR("Failed to configure BME280");
}
}

int32_t BME280Driver::readRaw()
{
    uint8_t buf[3] = {};
    if (!bus_.readRegs(REG_TEMP_MSB, buf, 3))
    {
        LOG_ERR("Temperature read failed");
        raw_ = INT32_MIN;
        return INT32_MIN;
    }
    // BME280 temperature ADC is 20-bit, stored in bits[19:0]
    raw_ = (static_cast<int32_t>(buf[0]) << 12) |
           (static_cast<int32_t>(buf[1]) << 4) |
           (static_cast<int32_t>(buf[2]) >> 4);
    return raw_;
}

// TODO: Implement full Bosch calibration.
// Currently using simplified conversion formula. Full driver would load
// calibration parameters T1/T2/T3 from 0x88 and apply compensation formula.
float BME280Driver::toSI()
{
    if (raw_ == INT32_MIN)
        return NAN;
    return static_cast<float>(raw_ - 524288) / 5120.0f;
}

uint8_t BME280Driver::deviceId()
{
    return bus_.address();
}

float BME280Driver::readTemperature_C()
{
    readRaw();
    return toSI();
}

// TODO: Implement full Bosch pressure calibration.
// Currently using simplified division by 4096 instead of calibration parameters.
float BME280Driver::readPressure_hPa()
{
    uint8_t buf[3] = {};
    if (!bus_.readRegs(REG_PRESS_MSB, buf, 3))
    {
        LOG_ERR("Pressure read failed");
        return NAN;
    }
    int32_t raw_p = (static_cast<int32_t>(buf[0]) << 12) |
                    (static_cast<int32_t>(buf[1]) << 4) |
                    (static_cast<int32_t>(buf[2]) >> 4);
    return static_cast<float>(raw_p) / 4096.0f;
}

// TODO: Implement full Bosch humidity calibration.
// Currently using simplified division by 1024 instead of calibration parameters.
float BME280Driver::readHumidity_pct()
{
    uint8_t buf[2] = {};
    if (!bus_.readRegs(REG_HUM_MSB, buf, 2))
        return NAN;
    int32_t raw_h = (static_cast<int32_t>(buf[0]) << 8) | buf[1];
    return static_cast<float>(raw_h) / 1024.0f;
}
