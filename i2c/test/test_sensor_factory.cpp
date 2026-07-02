#include "i2c_test_common.hpp"

TEST_F(I2CTest, Factory_CreatesBME280_DefaultAddr) {
    auto s = SensorFactory::create(SensorKind::BME280);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->deviceId(), BME280Driver::DEFAULT_ADDR);
}

TEST_F(I2CTest, Factory_CreatesSHTC3_DefaultAddr) {
    auto s = SensorFactory::create(SensorKind::SHTC3);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->deviceId(), SHTC3Driver::DEFAULT_ADDR);
}

TEST_F(I2CTest, Factory_CreatesLPS22HB_DefaultAddr) {
    auto s = SensorFactory::create(SensorKind::LPS22HB);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->deviceId(), LPS22HbDriver::DEFAULT_ADDR);
}

TEST_F(I2CTest, Factory_CreatesPAV3015_DefaultAddr) {
    auto s = SensorFactory::create(SensorKind::PAV3015);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->deviceId(), PAV3015Driver::DEFAULT_ADDR);
}

TEST_F(I2CTest, Factory_CustomAddressPassedThrough) {
    auto s = SensorFactory::create(SensorKind::BME280, 0x77);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->deviceId(), 0x77u);
}

TEST_F(I2CTest, Factory_CreatesLPS22HB_WritesODR_RecordsTransaction) {
    i2c_sim::transaction_history.clear();
    auto s = SensorFactory::create(SensorKind::LPS22HB, 0x5C);
    ASSERT_NE(s, nullptr);
    
    // Assert that the I2C transaction occurred and is recorded
    ASSERT_GE(i2c_sim::transaction_history.size(), 1u);
    EXPECT_EQ(i2c_sim::transaction_history[0].address, 0x5C);
    EXPECT_EQ(i2c_sim::transaction_history[0].reg, 0x10);
    EXPECT_TRUE(i2c_sim::transaction_history[0].is_write);
    EXPECT_EQ(i2c_sim::transaction_history[0].length, 1u);
}
