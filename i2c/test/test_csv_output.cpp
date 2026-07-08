#include "i2c_test_common.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

extern void i2c_app_init();
extern void i2c_app_sample_pav();
extern void i2c_app_sample_lps();
extern void i2c_app_sample_ambient();

TEST_F(I2CTest, CSVOutputVerification) {
    // 1. Initialize the application
    i2c_app_init();

    // 2. Set up mock sensor registers
    // PAV3015: Address 0x28
    setReg(0x28, pav3015_reg::STATUS, 0x01);
    setReg(0x28, pav3015_reg::DATA_H, 0x96); // 2415 >> 4 = 150 = 0x96
    setReg(0x28, pav3015_reg::DATA_L, 0xF0); // (2415 & 0x0F) << 4 = 0xF0

    // LPS22HB: Address 0x5C
    setReg24_LE(0x5C, 0x28, 4096u); // 4096/4096 = 1 hPa
    i2c_sim::regs[0x5C][0x2B] = 0xC4;
    i2c_sim::regs[0x5C][0x2C] = 0x09; // 25.0 C

    // BME280: Address 0x76
    setReg(0x76, 0xFA, 0x80);
    setReg(0x76, 0xFB, 0x00);
    setReg(0x76, 0xFC, 0x00); // 0 C
    setReg(0x76, 0xF7, 0x01);
    setReg(0x76, 0xF8, 0x00);
    setReg(0x76, 0xF9, 0x00); // 1.0 hPa
    setReg16_BE(0x76, 0xFD, 1024); // 1.0%

    // SHTC3: Address 0x70
    setReg16_BE(0x70, 0x00, 32768); // ~42.5 C
    setReg16_BE(0x70, 0x02, 32768); // 50%

    // 3. Set mock uptime
    mock_uptime_ms = 987654;

    // 4. Clear fake UART stream and sample each sensor group
    FakeUart::clear();
    i2c_app_sample_pav();
    i2c_app_sample_lps();
    i2c_app_sample_ambient();

    // 5. Verify CSV output contains expected sensor data
    std::string output = FakeUart::str();

    // Verify PAV3015 CSV line is present
    EXPECT_NE(output.find("PAV,987654"), std::string::npos)
        << "Missing PAV CSV line in output:\n" << output;

    // Verify LPS22HB CSV line is present
    EXPECT_NE(output.find("LPS,987654"), std::string::npos)
        << "Missing LPS CSV line in output:\n" << output;

    // Verify BME280 CSV line is present
    EXPECT_NE(output.find("BME,987654"), std::string::npos)
        << "Missing BME CSV line in output:\n" << output;

    // Verify SHTC3 CSV line is present
    EXPECT_NE(output.find("SHT,987654"), std::string::npos)
        << "Missing SHT CSV line in output:\n" << output;
}
