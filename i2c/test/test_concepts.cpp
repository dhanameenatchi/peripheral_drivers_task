#include "sensor_factory.hpp"
#include <gtest/gtest.h>

static_assert(SensorType_c<BME280Driver>,   "BME280 must satisfy Sensor concept");
static_assert(SensorType_c<SHTC3Driver>,    "SHTC3 must satisfy Sensor concept");
static_assert(SensorType_c<LPS22HbDriver>,  "LPS22HB must satisfy Sensor concept");
static_assert(SensorType_c<PAV3015Driver>,  "PAV3015 must satisfy Sensor concept");

TEST(I2CConcepts, ConceptsVerifiedAtCompileTime) {
    SUCCEED();
}
