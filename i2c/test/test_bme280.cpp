#include "i2c_test_common.hpp"

TEST_F(I2CTest, BME280_MidScaleRaw_IsNearZeroC) {
    // raw=524288 → (524288-524288)/5120 = 0°C
    setReg(0x76, 0xFA, 0x80);
    setReg(0x76, 0xFB, 0x00);
    setReg(0x76, 0xFC, 0x00);
    BME280Driver bme;
    bme.readRaw();
    EXPECT_NEAR(bme.toSI(), 0.0f, 1.0f);
}

TEST_F(I2CTest, BME280_FullScaleRaw_IsPositiveTemp) {
    // raw=0xFFFFF (20-bit max) → (1048575-524288)/5120 ≈ 102.4°C
    setReg(0x76, 0xFA, 0xFF);
    setReg(0x76, 0xFB, 0xFF);
    setReg(0x76, 0xFC, 0xF0);
    BME280Driver bme;
    bme.readRaw();
    EXPECT_GT(bme.toSI(), 50.0f);
}

TEST_F(I2CTest, BME280_ReadTemperature_C_ReturnsValue) {
    setReg(0x76, 0xFA, 0x80);
    setReg(0x76, 0xFB, 0x00);
    setReg(0x76, 0xFC, 0x00);
    BME280Driver bme;
    float t = bme.readTemperature_C();
    EXPECT_NEAR(t, 0.0f, 1.0f);
}

TEST_F(I2CTest, BME280_ReadPressure_SuccessReturnsValue) {
    setReg(0x76, 0xF7, 0x01);
    setReg(0x76, 0xF8, 0x00);
    setReg(0x76, 0xF9, 0x00);
    BME280Driver bme;
    float p = bme.readPressure_hPa();
    EXPECT_NEAR(p, 1.0f, 0.01f);
}

TEST_F(I2CTest, BME280_ReadHumidity_SuccessReturnsValue) {
    setReg16_BE(0x76, 0xFD, 1024);
    BME280Driver bme;
    float h = bme.readHumidity_pct();
    EXPECT_NEAR(h, 1.0f, 0.01f);
}
