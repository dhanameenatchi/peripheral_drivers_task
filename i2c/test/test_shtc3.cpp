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

TEST_F(I2CTest, SHTC3_CrcFailures_Address71) {
    SHTC3Driver shtc(0x71);

    // 1. Temperature CRC failure
    i2c_sim::regs[0x71][0] = 0x12;
    i2c_sim::regs[0x71][1] = 0x34;
    i2c_sim::regs[0x71][2] = 0x00; // wrong CRC
    EXPECT_EQ(shtc.readRaw(), INT32_MIN);

    // 2. Humidity CRC failure
    i2c_sim::regs[0x71][3] = 0x56;
    i2c_sim::regs[0x71][4] = 0x78;
    i2c_sim::regs[0x71][5] = 0x00; // wrong CRC
    EXPECT_TRUE(std::isnan(shtc.readHumidity_pct()));
    
    // 3. Valid CRC test for custom address 0x71
    uint8_t t_data[2] = {0x12, 0x34};
    uint8_t t_crc = SHTC3Driver::calculateCRC(t_data, 2);
    i2c_sim::regs[0x71][0] = 0x12;
    i2c_sim::regs[0x71][1] = 0x34;
    i2c_sim::regs[0x71][2] = t_crc;

    uint8_t h_data[2] = {0x56, 0x78};
    uint8_t h_crc = SHTC3Driver::calculateCRC(h_data, 2);
    i2c_sim::regs[0x71][3] = 0x56;
    i2c_sim::regs[0x71][4] = 0x78;
    i2c_sim::regs[0x71][5] = h_crc;

    EXPECT_EQ(shtc.readRaw(), 0x1234);
    EXPECT_NEAR(shtc.readHumidity_pct(), 100.0f * static_cast<float>(0x5678) / 65536.0f, 0.01f);
}

