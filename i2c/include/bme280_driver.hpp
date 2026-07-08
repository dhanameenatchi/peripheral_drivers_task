#pragma once
#include "sensor.hpp"
#include <cstdint>

class BME280Driver final : public ISensor
{
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x76;
    static constexpr uint8_t REG_CHIP_ID  = 0xD0;
    static constexpr uint8_t CHIP_ID      = 0x60;

    static constexpr uint8_t REG_TEMP_MSB  = 0xFA;
    static constexpr uint8_t REG_PRESS_MSB = 0xF7;
    static constexpr uint8_t REG_HUM_MSB   = 0xFD;

    static constexpr uint8_t REG_CTRL_HUM  = 0xF2; // must be written BEFORE ctrl_meas
    static constexpr uint8_t REG_CTRL_MEAS = 0xF4;

    // Calibration data locations (Table 16, datasheet §4.2.2)
    static constexpr uint8_t REG_CALIB00 = 0x88; // dig_T1..dig_P9, 24 bytes (0x88..0x9F)
    static constexpr uint8_t REG_CALIB_H1 = 0xA1; // dig_H1, 1 byte
    static constexpr uint8_t REG_CALIB26 = 0xE1; // dig_H2..dig_H6, 7 bytes (0xE1..0xE7)

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
    // Calibration trimming parameters (Table 16, datasheet §4.2.2)
    // Types match the datasheet exactly: unsigned short / signed short / unsigned char / signed char
    struct CalibData
    {
        uint16_t dig_T1 = 0;
        int16_t  dig_T2 = 0;
        int16_t  dig_T3 = 0;

        uint16_t dig_P1 = 0;
        int16_t  dig_P2 = 0;
        int16_t  dig_P3 = 0;
        int16_t  dig_P4 = 0;
        int16_t  dig_P5 = 0;
        int16_t  dig_P6 = 0;
        int16_t  dig_P7 = 0;
        int16_t  dig_P8 = 0;
        int16_t  dig_P9 = 0;

        uint8_t dig_H1 = 0;
        int16_t dig_H2 = 0;
        uint8_t dig_H3 = 0;
        int16_t dig_H4 = 0;
        int16_t dig_H5 = 0;
        int8_t  dig_H6 = 0;
    };

    bool readCalibrationData();

    I2cBus bus_;
    int32_t raw_ = 0;
    CalibData calib_{};
    int32_t t_fine_ = 0; // carries fine-resolution temperature into P/H compensation
    bool calib_valid_ = false;

    // Compensation formulas (datasheet §4.2.3, integer/int64 variant)
    int32_t compensateTemperature(int32_t adc_T);
    uint32_t compensatePressure(int32_t adc_P);
    uint32_t compensateHumidity(int32_t adc_H);
};

static_assert(SensorType_c<BME280Driver>, "BME280Driver must satisfy Sensor concept");