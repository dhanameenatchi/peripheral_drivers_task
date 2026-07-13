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
    // PAV3015: Address 0x28, set ADC = 2187 to get exactly 5.000 m/s
    uint16_t adc_pav = 2187;
    i2c_sim::regs[0x28][1] = static_cast<uint8_t>(adc_pav >> 8);
    i2c_sim::regs[0x28][2] = static_cast<uint8_t>(adc_pav & 0xFF);
    i2c_sim::regs[0x28][3] = 0;
    i2c_sim::regs[0x28][4] = 0;
    i2c_sim::regs[0x28][0] = static_cast<uint8_t>(256 - (i2c_sim::regs[0x28][1] + i2c_sim::regs[0x28][2]));

    // LPS22HB: Address 0x5D (address used in the application)
    setReg24_LE(0x5D, 0x28, 4096u); // 4096/4096 = 1 hPa
    i2c_sim::regs[0x5D][0x2B] = 0xC4;
    i2c_sim::regs[0x5D][0x2C] = 0x09; // 25.0 C

    // BME280: Address 0x76
    setReg(0x76, 0xFA, 0x80);
    setReg(0x76, 0xFB, 0x00);
    setReg(0x76, 0xFC, 0x00); // 0 C
    setReg(0x76, 0xF7, 0x01);
    setReg(0x76, 0xF8, 0x00);
    setReg(0x76, 0xF9, 0x00); // 1658.90 hPa (under updated calibration)
    setReg16_BE(0x76, 0xFD, 1024); // 5.7% (under updated calibration)

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
    
    EXPECT_NE(output.find("PAV,987654,2187,5.000"), std::string::npos);
    EXPECT_NE(output.find("LPS,987654,1.00,25.00"), std::string::npos);
    EXPECT_NE(output.find("BME,987654,0.00,1658.90,5.7"), std::string::npos);
    EXPECT_NE(output.find("SHT,987654,42.50,50.0"), std::string::npos);
}
