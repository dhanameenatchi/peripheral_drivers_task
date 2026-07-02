#include "i2c_test_common.hpp"

TEST_F(I2CTest, SHTC3_ZeroRaw_IsMinus45C) {
    setReg16_BE(0x70, 0x00, 0x0000);
    SHTC3Driver shtc;
    shtc.readRaw();
    EXPECT_NEAR(shtc.toSI(), -45.0f, 0.5f);
}

TEST_F(I2CTest, SHTC3_FullScaleRaw_Is130C) {
    // raw=65535 → -45 + 175 = 130°C
    setReg16_BE(0x70, 0x00, 0xFFFF);
    SHTC3Driver shtc;
    shtc.readRaw();
    EXPECT_NEAR(shtc.toSI(), 130.0f, 0.5f);
}

TEST_F(I2CTest, SHTC3_ReadTemperature_C_ReturnsValue) {
    setReg16_BE(0x70, 0x00, 0x8000);
    SHTC3Driver shtc;
    float t = shtc.readTemperature_C();
    EXPECT_GT(t, -50.0f);
}

TEST_F(I2CTest, SHTC3_ReadHumidity_SuccessReturnsValue) {
    setReg16_BE(0x70, 0x02, 32768);
    SHTC3Driver shtc;
    float h = shtc.readHumidity_pct();
    EXPECT_NEAR(h, 50.0f, 0.5f);
}
