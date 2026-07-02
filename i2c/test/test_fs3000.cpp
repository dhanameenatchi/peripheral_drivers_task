#include "i2c_test_common.hpp"

TEST_F(I2CTest, PAV3015_Checksum_Valid)
{
    uint8_t buf[5] = {0, 0x04, 0xDE, 0x00, 0x00};
    buf[0] = buf[1] + buf[2] + buf[3] + buf[4]; // = 0xE2
    EXPECT_TRUE(PAV3015Driver::validateChecksum(buf, 5));
}

TEST_F(I2CTest, PAV3015_Checksum_Invalid)
{
    uint8_t buf[5] = {0xAA, 0x04, 0xDE, 0x00, 0x00};
    EXPECT_FALSE(PAV3015Driver::validateChecksum(buf, 5));
}

TEST_F(I2CTest, PAV3015_Checksum_TooShort)
{
    uint8_t buf[1] = {0x00};
    EXPECT_FALSE(PAV3015Driver::validateChecksum(buf, 1));
}

TEST_F(I2CTest, PAV3015_Checksum_ZeroBytes)
{
    uint8_t buf[3] = {0x00, 0x00, 0x00};
    EXPECT_TRUE(PAV3015Driver::validateChecksum(buf, 3));
}

TEST_F(I2CTest, PAV3015_Interpolation_AtAllKnots_15MPS)
{
    for (const auto &knot : PAV3015_KNOTS_15MPS)
    {
        float v = PAV3015Driver::interpolate15(knot.raw);
        EXPECT_NEAR(v, knot.velocity_mps, 0.001f) << "Failed at raw=" << knot.raw;
    }
}

TEST_F(I2CTest, PAV3015_Interpolation_AtAllKnots_7MPS)
{
    for (const auto &knot : PAV3015_KNOTS_7MPS)
    {
        float v = PAV3015Driver::interpolate7(knot.raw);
        EXPECT_NEAR(v, knot.velocity_mps, 0.001f) << "Failed at raw=" << knot.raw;
    }
}

TEST_F(I2CTest, PAV3015_Interpolation_AtMidpoints_15MPS)
{
    const auto &tbl = PAV3015_KNOTS_15MPS;
    for (size_t i = 1; i < tbl.size(); ++i)
    {
        uint16_t mid = static_cast<uint16_t>((tbl[i - 1].raw + tbl[i].raw) / 2);
        float v = PAV3015Driver::interpolate15(mid);
        EXPECT_GE(v, tbl[i - 1].velocity_mps);
        EXPECT_LE(v, tbl[i].velocity_mps);
    }
}

TEST_F(I2CTest, PAV3015_Clamp_BelowMin_Returns0mps)
{
    float v = PAV3015Driver::interpolate15(0);
    EXPECT_NEAR(v, PAV3015_KNOTS_15MPS.front().velocity_mps, 0.001f);
}

TEST_F(I2CTest, PAV3015_Clamp_AboveMax_Returns15mps)
{
    float v = PAV3015Driver::interpolate15(0xFFFF);
    EXPECT_NEAR(v, PAV3015_KNOTS_15MPS.back().velocity_mps, 0.001f);
}

TEST_F(I2CTest, PAV3015_Clamp_AtExactMin_Returns0mps)
{
    float v = PAV3015Driver::interpolate15(pav3015_reg::RAW_MIN);
    EXPECT_NEAR(v, 0.0f, 0.001f);
}

TEST_F(I2CTest, PAV3015_Clamp_AtExactMax_Returns15mps)
{
    float v = PAV3015Driver::interpolate15(pav3015_reg::RAW_MAX);
    EXPECT_NEAR(v, 15.0f, 0.001f);
}

static void setPav3015Adc(uint16_t adc)
{
    constexpr uint8_t addr = PAV3015Driver::DEFAULT_ADDR;
    i2c_sim::regs[addr][pav3015_reg::STATUS] = 0x01;
    i2c_sim::regs[addr][pav3015_reg::DATA_H] = static_cast<uint8_t>(0xF0 | ((adc >> 8) & 0x0F));
    i2c_sim::regs[addr][pav3015_reg::DATA_L] = static_cast<uint8_t>(adc & 0xFF);
}

TEST_F(I2CTest, PAV3015_ReadVelocity_AtFirstKnot_Is0mps)
{
    setPav3015Adc(PAV3015_KNOTS_15MPS[0].raw);
    PAV3015Driver pav;
    EXPECT_NEAR(pav.readVelocity_mps(), 0.0f, 0.01f);
}

TEST_F(I2CTest, PAV3015_ReadVelocity_AtLastKnot_Is15mps)
{
    setPav3015Adc(PAV3015_KNOTS_15MPS.back().raw);
    PAV3015Driver pav;
    EXPECT_NEAR(pav.readVelocity_mps(), 15.0f, 0.01f);
}

TEST_F(I2CTest, PAV3015_ReadVelocity_MidKnot)
{
    setPav3015Adc(2415);
    PAV3015Driver pav;
    EXPECT_NEAR(pav.readVelocity_mps(), 5.0f, 0.05f);
}

TEST_F(I2CTest, PAV3015_ReadRaw_DecodesCorrectly)
{
    constexpr uint16_t adc = 1234;
    setPav3015Adc(adc);
    PAV3015Driver pav;
    EXPECT_EQ(pav.readRaw(), static_cast<int32_t>(adc));
}

TEST_F(I2CTest, PAV3015_Range7MPS_WritesCTRL)
{
    PAV3015Driver pav(PAV3015Driver::DEFAULT_ADDR, PAV3015Driver::RANGE_7MPS);
    EXPECT_EQ(i2c_sim::regs[PAV3015Driver::DEFAULT_ADDR][pav3015_reg::CTRL],
              pav3015_reg::RANGE_7MPS);
}

TEST_F(I2CTest, PAV3015_SetRange_ChangesInterpolationTable)
{
    setPav3015Adc(PAV3015_KNOTS_7MPS.back().raw);
    PAV3015Driver pav(PAV3015Driver::DEFAULT_ADDR, PAV3015Driver::RANGE_7MPS);
    EXPECT_NEAR(pav.readVelocity_mps(),
                PAV3015_KNOTS_7MPS.back().velocity_mps, 0.01f);
}
