#include "i2c_test_common.hpp"

// ---------------------------------------------------------------------------
// Helper: pre-populate WHO_AM_I so the constructor doesn't bail out
// ---------------------------------------------------------------------------
static void setupLps22hb(uint8_t addr = 0x5C) {
    i2c_sim::regs[addr][LPS22HbDriver::REG_WHO_AM_I] = LPS22HbDriver::CHIP_ID;
}

TEST_F(I2CTest, LPS22HB_ODR3_WritesCorrectCtrlReg) {
    setupLps22hb();
    LPS22HbDriver lps(0x5C, 3);
    EXPECT_EQ(i2c_sim::regs[0x5C][0x10], 0x30u);
}

TEST_F(I2CTest, LPS22HB_ODR4_WritesCorrectCtrlReg) {
    setupLps22hb();
    LPS22HbDriver lps(0x5C, 4);
    EXPECT_EQ(i2c_sim::regs[0x5C][0x10], 0x40u);
}

TEST_F(I2CTest, LPS22HB_ReadPressure_1hPa) {
    setupLps22hb();
    // raw = 4096 → 4096/4096 = 1.0 hPa; 4096 = 0x001000
    setReg24_LE(0x5C, 0x28, 4096u);
    LPS22HbDriver lps;
    EXPECT_NEAR(lps.readPressure_hPa(), 1.0f, 0.01f);
}

TEST_F(I2CTest, LPS22HB_ToSI_Returns_Pa) {
    setupLps22hb();
    // 1 hPa → 100 Pa
    setReg24_LE(0x5C, 0x28, 4096u);
    LPS22HbDriver lps;
    lps.readRaw();
    EXPECT_NEAR(lps.toSI(), 100.0f, 0.5f);
}

TEST_F(I2CTest, LPS22HB_StandardAtmosphere_hPa_to_Pa) {
    setupLps22hb();
    setReg24_LE(0x5C, 0x28, 4150272u);
    LPS22HbDriver lps;
    lps.readRaw();
    EXPECT_NEAR(lps.toSI(), 101325.0f, 500.0f);
}

TEST_F(I2CTest, LPS22HB_MovingAverage_RampInput) {
    setupLps22hb();
    LPS22HbDriver lps;
    float pressures[] = {10.0f, 20.0f, 30.0f, 40.0f};
    float last = 0.0f;
    for (float p : pressures) {
        setReg24_LE(0x5C, 0x28, static_cast<uint32_t>(p * 4096.0f));
        last = lps.readPressureFiltered_hPa();
    }
    // (10+20+30+40)/4 = 25.0 hPa
    EXPECT_NEAR(last, 25.0f, 0.5f);
}

TEST_F(I2CTest, LPS22HB_MovingAverage_ImpulseDecays) {
    setupLps22hb();
    LPS22HbDriver lps;
    auto setP = [&](float p) {
        setReg24_LE(0x5C, 0x28, static_cast<uint32_t>(p * 4096.0f));
    };
    setP(100.0f); float f1 = lps.readPressureFiltered_hPa();
    setP(  0.0f); float f2 = lps.readPressureFiltered_hPa();
    setP(  0.0f); float f3 = lps.readPressureFiltered_hPa();
    setP(  0.0f); float f4 = lps.readPressureFiltered_hPa();
    setP(  0.0f); float f5 = lps.readPressureFiltered_hPa();

    EXPECT_GT(f1, f2);
    EXPECT_GT(f2, f3);
    EXPECT_GT(f3, f4);
    EXPECT_NEAR(f5, 0.0f, 0.1f);
}

TEST_F(I2CTest, LPS22HB_MovingAverage_PartialFill) {
    setupLps22hb();
    LPS22HbDriver lps;
    setReg24_LE(0x5C, 0x28, static_cast<uint32_t>(10.0f * 4096.0f));
    float f1 = lps.readPressureFiltered_hPa();
    EXPECT_NEAR(f1, 10.0f, 0.1f);

    setReg24_LE(0x5C, 0x28, static_cast<uint32_t>(20.0f * 4096.0f));
    float f2 = lps.readPressureFiltered_hPa();
    EXPECT_NEAR(f2, 15.0f, 0.1f);
}

TEST_F(I2CTest, LPS22HB_ReadTemp_SuccessReturnsValue) {
    setupLps22hb();
    i2c_sim::regs[0x5C][0x2B] = 0xC4;
    i2c_sim::regs[0x5C][0x2C] = 0x09;
    LPS22HbDriver lps;
    float t = lps.readTemp_C();
    EXPECT_NEAR(t, 25.0f, 0.1f);
}

TEST_F(I2CTest, LPS22HB_ReadPressure_NegativeSignExtension) {
    setupLps22hb();
    setReg24_LE(0x5C, 0x28, 0xFFFFFFu);
    LPS22HbDriver lps;
    int32_t raw = lps.readRaw();
    EXPECT_EQ(raw, -1);
}

