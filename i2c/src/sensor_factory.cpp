#include "sensor_factory.hpp"

std::unique_ptr<ISensor> SensorFactory::create(SensorKind kind, uint8_t addr) {
    switch (kind) {
        case SensorKind::BME280:
            return std::make_unique<BME280Driver>(
                addr ? addr : BME280Driver::DEFAULT_ADDR);
        case SensorKind::SHTC3:
            return std::make_unique<SHTC3Driver>(
                addr ? addr : SHTC3Driver::DEFAULT_ADDR);
        case SensorKind::LPS22HB:
            return std::make_unique<LPS22HbDriver>(
                addr ? addr : LPS22HbDriver::DEFAULT_ADDR);
        case SensorKind::PAV3015:
            return std::make_unique<PAV3015Driver>(
                addr ? addr : PAV3015Driver::DEFAULT_ADDR);
    }
    return nullptr;
}
