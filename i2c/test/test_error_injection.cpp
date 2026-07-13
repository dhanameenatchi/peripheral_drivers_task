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
    // Corrupt checksum in mock registers so the second read (triggered by toSI()) fails validation.
    i2c_sim::regs[PAV3015Driver::DEFAULT_ADDR][0] = 0xFF;
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
    PAV3015Driver pav;
    i2c_sim::nack_next = true;
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

// ---------------------------------------------------------------------------
// Additional error injection tests
// ---------------------------------------------------------------------------
TEST_F(I2CTest, ConstructorNack_BME280_ChipIDReadFails) {
    i2c_sim::nack_next = true;
    BME280Driver bme;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 1u);
}

TEST_F(I2CTest, ConstructorNack_BME280_UnexpectedChipID) {
    i2c_sim::regs[BME280Driver::DEFAULT_ADDR][BME280Driver::REG_CHIP_ID] = 0x00;
    BME280Driver bme;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 1u);
}

TEST_F(I2CTest, ConstructorNack_BME280_CalibReadFails) {
    i2c_sim::nack_after_n = 1;
    BME280Driver bme;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 2u);
}

TEST_F(I2CTest, ConstructorNack_BME280_CtrlHumFails) {
    i2c_sim::nack_after_n = 2;
    BME280Driver bme;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 3u);
}

TEST_F(I2CTest, ConstructorNack_BME280_CtrlMeasFails) {
    i2c_sim::nack_after_n = 3;
    BME280Driver bme;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 4u);
}

TEST_F(I2CTest, ConstructorNack_SHTC3_WakeFails) {
    i2c_sim::nack_next = true;
    SHTC3Driver shtc;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 1u);
}

TEST_F(I2CTest, ConstructorNack_SHTC3_ResetFails) {
    i2c_sim::nack_after_n = 1;
    SHTC3Driver shtc;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 2u);
}

TEST_F(I2CTest, ConstructorNack_LPS22HB_WhoAmIFails) {
    i2c_sim::nack_next = true;
    LPS22HbDriver lps;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 1u);
}

TEST_F(I2CTest, ConstructorNack_LPS22HB_Ctrl1Fails) {
    i2c_sim::nack_after_n = 1;
    LPS22HbDriver lps;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 2u);
}

TEST_F(I2CTest, ConstructorNack_PAV3015_RangeReadFails) {
    i2c_sim::nack_next = true;
    PAV3015Driver pav;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 1u);
}


TEST_F(I2CTest, SHTC3_ReadRaw_WakeFails) {
    SHTC3Driver shtc;
    i2c_sim::nack_next = true;
    EXPECT_EQ(shtc.readRaw(), INT32_MIN);
}

TEST_F(I2CTest, SHTC3_ReadRaw_MeasurementFails) {
    SHTC3Driver shtc;
    i2c_sim::nack_after_n = 1;
    EXPECT_EQ(shtc.readRaw(), INT32_MIN);
}

TEST_F(I2CTest, SHTC3_ReadHumidity_WakeFails) {
    SHTC3Driver shtc;
    i2c_sim::nack_next = true;
    EXPECT_TRUE(std::isnan(shtc.readHumidity_pct()));
}

TEST_F(I2CTest, SHTC3_ReadHumidity_MeasurementFails) {
    SHTC3Driver shtc;
    i2c_sim::nack_after_n = 1;
    EXPECT_TRUE(std::isnan(shtc.readHumidity_pct()));
}

TEST_F(I2CTest, ConstructorNack_LPS22HB_UnexpectedChipID) {
    i2c_sim::regs[0x5C][0x0F] = 0x00;
    LPS22HbDriver lps;
    EXPECT_EQ(i2c_sim::transaction_history.size(), 1u);
}

TEST_F(I2CTest, PAV3015_SetRange_InvalidRangeReturnsFalse) {
    PAV3015Driver pav;
    EXPECT_FALSE(pav.setRange(99));
}

TEST_F(I2CTest, SHTC3_ReadRaw_ReadBytesFails) {
    SHTC3Driver shtc;
    i2c_sim::nack_after_n = 2;
    EXPECT_EQ(shtc.readRaw(), INT32_MIN);
}

TEST_F(I2CTest, SHTC3_ReadHumidity_ReadBytesFails) {
    SHTC3Driver shtc;
    i2c_sim::nack_after_n = 2;
    EXPECT_TRUE(std::isnan(shtc.readHumidity_pct()));
}

TEST_F(I2CTest, SHTC3_ReadTemperature_NackReturnsNaN) {
    SHTC3Driver shtc;
    i2c_sim::nack_next = true;
    EXPECT_TRUE(std::isnan(shtc.readTemperature_C()));
}


