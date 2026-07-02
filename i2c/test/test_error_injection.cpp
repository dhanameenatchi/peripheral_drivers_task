#include "i2c_test_common.hpp"

TEST_F(I2CTest, Nack_BME280_ReadRawReturnsMin) {
    BME280Driver bme;
    i2c_sim::nack_next = true;
    EXPECT_EQ(bme.readRaw(), INT32_MIN);
}

TEST_F(I2CTest, Nack_BME280_ToSIReturnsNaN) {
    BME280Driver bme;
    i2c_sim::nack_next = true;
    bme.readRaw();
    EXPECT_TRUE(std::isnan(bme.toSI()));
}

TEST_F(I2CTest, Nack_SHTC3_ReadRawReturnsMin) {
    SHTC3Driver shtc;
    i2c_sim::nack_next = true;
    EXPECT_EQ(shtc.readRaw(), INT32_MIN);
}

TEST_F(I2CTest, Nack_LPS22HB_ReadRawReturnsMin) {
    LPS22HbDriver lps;
    i2c_sim::nack_next = true;
    EXPECT_EQ(lps.readRaw(), INT32_MIN);
}

TEST_F(I2CTest, Nack_PAV3015_ReadRawReturnsMin) {
    PAV3015Driver pav;
    i2c_sim::nack_next = true;
    EXPECT_EQ(pav.readRaw(), INT32_MIN);
}

TEST_F(I2CTest, Nack_PAV3015_ReadVelocityReturnsNaN) {
    PAV3015Driver pav;
    i2c_sim::nack_next = true;
    EXPECT_TRUE(std::isnan(pav.readVelocity_mps()));
}

TEST_F(I2CTest, SHTC3_NackToSI_IsNaN) {
    SHTC3Driver shtc;
    i2c_sim::nack_next = true;
    shtc.readRaw();
    EXPECT_TRUE(std::isnan(shtc.toSI()));
}

TEST_F(I2CTest, PAV3015_DataH_Nack_ReturnsMin) {
    i2c_sim::regs[PAV3015Driver::DEFAULT_ADDR][pav3015_reg::STATUS] = 0x01;
    PAV3015Driver pav;
    i2c_sim::nack_after_n = 1;
    EXPECT_EQ(pav.readRaw(), INT32_MIN);
}

TEST_F(I2CTest, PAV3015_ToSI_ReturnsNaN_OnNack) {
    PAV3015Driver pav;
    i2c_sim::nack_next = true;
    float val = pav.toSI();
    EXPECT_TRUE(std::isnan(val));
}

TEST_F(I2CTest, BME280_ReadPressure_Nack_ReturnsNaN) {
    BME280Driver bme;
    i2c_sim::nack_next = true;
    EXPECT_TRUE(std::isnan(bme.readPressure_hPa()));
}

TEST_F(I2CTest, BME280_ReadHumidity_Nack_ReturnsNaN) {
    BME280Driver bme;
    i2c_sim::nack_next = true;
    EXPECT_TRUE(std::isnan(bme.readHumidity_pct()));
}

TEST_F(I2CTest, SHTC3_ReadHumidity_Nack_ReturnsNaN) {
    SHTC3Driver shtc;
    i2c_sim::nack_next = true;
    EXPECT_TRUE(std::isnan(shtc.readHumidity_pct()));
}

TEST_F(I2CTest, LPS22HB_ReadPressure_Nack_ReturnsNaN) {
    LPS22HbDriver lps;
    i2c_sim::nack_next = true;
    EXPECT_TRUE(std::isnan(lps.readPressure_hPa()));
}

TEST_F(I2CTest, LPS22HB_ReadTemp_Nack_ReturnsNaN) {
    LPS22HbDriver lps;
    i2c_sim::nack_next = true;
    EXPECT_TRUE(std::isnan(lps.readTemp_C()));
}

TEST_F(I2CTest, LPS22HB_ReadPressureFiltered_Nack_ReturnsNaN) {
    LPS22HbDriver lps;
    i2c_sim::nack_next = true;
    float f = lps.readPressureFiltered_hPa();
    EXPECT_TRUE(std::isnan(f));
}

TEST_F(I2CTest, LPS22HB_ToSI_NackReturnsNaN) {
    LPS22HbDriver lps;
    i2c_sim::nack_next = true;
    lps.readRaw();
    EXPECT_TRUE(std::isnan(lps.toSI()));
}
