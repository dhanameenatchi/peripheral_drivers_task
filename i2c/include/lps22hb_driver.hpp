#pragma once
#include "sensor.hpp"
#include <array>

class LPS22HbDriver final : public ISensor
{
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x5C;
    static constexpr uint8_t REG_CTRL1 = 0x10;
    static constexpr uint8_t REG_PRESS_XL = 0x28;
    static constexpr uint8_t REG_TEMP_L = 0x2B;
    static constexpr uint8_t REG_WHO_AM_I = 0x0F;
    static constexpr uint8_t CHIP_ID = 0xB1;

    explicit LPS22HbDriver(uint8_t addr = DEFAULT_ADDR, uint8_t odr = 3);

    void configODR(uint8_t odr);

    // ISensor
    int32_t readRaw() override;
    float toSI() override;
    uint8_t deviceId() override;

    // Extended API
    float readPressure_hPa();
    float readTemp_C();
    float readPressureFiltered_hPa();

private:
    static constexpr size_t AVG_TAPS = 4;

    I2cBus bus_;
    int32_t raw_ = 0;
    uint8_t odr_ = 3;
    std::array<float, AVG_TAPS> avg_buf_{};
    size_t avg_idx_ = 0;
    size_t avg_count_ = 0;
};

static_assert(SensorType_c<LPS22HbDriver>, "LPS22HbDriver must satisfy Sensor concept");
