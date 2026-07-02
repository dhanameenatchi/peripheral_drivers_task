#pragma once
#include "sensor.hpp"

class SHTC3Driver final : public ISensor {
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x70;

    // 16-bit Commands
    static constexpr uint16_t CMD_WAKEUP            = 0x3517;
    static constexpr uint16_t CMD_SLEEP             = 0xB098;
    static constexpr uint16_t CMD_MEAS_T_FIRST_POLL = 0x7866;
    static constexpr uint16_t CMD_SOFT_RESET        = 0x805D;

    explicit SHTC3Driver(uint8_t addr = DEFAULT_ADDR);

    // ISensor
    int32_t readRaw() override;
    float   toSI() override;
    uint8_t deviceId() override;

    // Extended API
    float readTemperature_C();
    float readHumidity_pct();

    // CRC calculation helper
    static uint8_t calculateCRC(const uint8_t* data, size_t len);

private:
    I2cBus  bus_;
    int32_t raw_ = 0;
};

static_assert(SensorType_c<SHTC3Driver>, "SHTC3Driver must satisfy Sensor concept");
