#pragma once
// =============================================================================
// sensor_factory.hpp — I2C Sensor C++20 concept + Factory Method Pattern
// SensorFactory::create(SensorKind, addr) → std::unique_ptr<ISensor>
// Drivers: BME280 (0x76), SHTC3 (0x70), LPS22HB (0x5C), PAV3015 (0x28)
// =============================================================================

#include "sensor.hpp"
#include "bme280_driver.hpp"
#include "shtc3_driver.hpp"
#include "lps22hb_driver.hpp"
#include "pav3015_driver.hpp"
#include <memory>

enum class SensorKind { BME280, SHTC3, LPS22HB, PAV3015 };

class SensorFactory {
public:
    static std::unique_ptr<ISensor> create(SensorKind kind, uint8_t addr = 0);
};