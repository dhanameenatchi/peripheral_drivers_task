#pragma once
#include "sensor.hpp"

class BME280Driver final : public ISensor
{
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x76;
    static constexpr uint8_t REG_CHIP_ID = 0xD0;
    static constexpr uint8_t CHIP_ID = 0x60;
    static constexpr uint8_t REG_TEMP_MSB = 0xFA;
    static constexpr uint8_t REG_PRESS_MSB = 0xF7;
    static constexpr uint8_t REG_HUM_MSB = 0xFD;
    static constexpr uint8_t REG_CTRL_MEAS = 0xF4;

    explicit BME280Driver(uint8_t addr = DEFAULT_ADDR);

    // ISensor
    int32_t readRaw() override;
    float toSI() override;
    uint8_t deviceId() override;

    // Extended API
    float readTemperature_C();
    float readPressure_hPa();
    float readHumidity_pct();

private:
    I2cBus bus_;
    int32_t raw_ = 0;
};

static_assert(SensorType_c<BME280Driver>, "BME280Driver must satisfy Sensor concept");
