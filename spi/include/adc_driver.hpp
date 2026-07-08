#pragma once
// =============================================================================
// SPI — ADS1118 ADC Driver + Strategy Pattern (IFilter)
// C++20, Zephyr SPI HAL wrapper
// Strategy swappable at runtime via UART command
// =============================================================================
#ifdef ZEPHYR_BUILD
  #include <zephyr/drivers/spi.h>
  #include <zephyr/drivers/gpio.h>
  #include <zephyr/device.h>
  #include <zephyr/devicetree.h>
#else
  #include "zephyr_spi_mock.hpp"
#endif

#include "filter.hpp"
#include "moving_average_filter.hpp"
#include "median_filter.hpp"
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// ADS1118 ADC Driver — SPI Mode 1 (CPOL=0, CPHA=1)
// 16-bit, differential + single-ended channels
// ---------------------------------------------------------------------------
class AdcDriver {
public:
    // Config register bits for ADS1118
    // Bit 15: Single-Shot Start Conversion
    enum class StartConv : uint16_t {
        NO_EFFECT = 0x0000,
        START     = 0x8000  // 1 << 15
    };

    // Bits 14:12: Input Multiplexer (MUX)
    enum class Channel : uint16_t {
        DIFF_01 = 0x0000,   // AIN0 vs AIN1 (Default)
        SE_AIN0 = 0x4000,   // AIN0 vs GND (100 in binary shifted left by 12)
        SE_AIN1 = 0x5000,   // AIN1 vs GND (101 in binary shifted left by 12)
        SE_AIN2 = 0x6000,   // AIN2 vs GND (110 in binary shifted left by 12)
        SE_AIN3 = 0x7000,   // AIN3 vs GND (111 in binary shifted left by 12)
    };

    // Bits 11:9: Programmable Gain Amplifier (PGA / FSR)
    enum class PGA : uint16_t {
        FSR_6_144V = 0x0000, // 000 in binary
        FSR_4_096V = 0x0200, // 001 in binary shifted left by 9
        FSR_2_048V = 0x0400, // 010 in binary shifted left by 9 (Default)
        FSR_1_024V = 0x0600, // 011 in binary shifted left by 9
    };

    // Bit 8: Device Operating Mode
    enum class Mode : uint16_t {
        CONTINUOUS  = 0x0000,
        SINGLE_SHOT = 0x0100  // 1 << 8
    };

    // Bits 7:5: Data Rate (SPS)
    enum class DataRate : uint16_t {
        SPS_8   = 0x0000,
        SPS_128 = 0x0080,    // 100 in binary shifted left by 5 (Default)
        SPS_860 = 0x00E0,    // 111 in binary shifted left by 5
    };

    // Bit 4: Operating Mode (ADC vs Temperature Sensor)
    enum class SensorMode : uint16_t {
        ADC_MODE  = 0x0000,
        TEMP_MODE = 0x0010   // 1 << 4
    };

    // Bit 3: DOUT/DRDY Pin Pull-Up Resistor Enable
    enum class PullUp : uint16_t {
        DISABLE = 0x0000,
        ENABLE  = 0x0008     // 1 << 3
    };

    // Bits 2:1: No Operation (NOP) Execution Verification Code
    enum class Nop : uint16_t {
        INVALID = 0x0000,
        VALID   = 0x0002     // 01 in binary shifted left by 1
    };

    // Bit 0: Reserved (Must always be written as 1)
    static constexpr uint16_t RESERVED_BIT = 0x0001;
    
    explicit AdcDriver(IFilter* filter = nullptr);

    void setFilter(IFilter* f);

    // Read raw ADC value from selected channel
    // int16_t readChannel(Channel ch, PGA pga = PGA::FSR_2V);
int16_t readChannel(Channel ch, PGA pga, Mode mode);
    // Apply current strategy filter to history
    int16_t filteredValue();

    [[nodiscard]] int  lastError() const;
    void clearHistory();
    [[nodiscard]] const std::vector<int16_t>& history() const;

private:
#ifdef ZEPHYR_BUILD
    // SPI Mode 1: CPOL=0, CPHA=1, MSB-first, 16-bit words
    // (ADS1118 transactions are exactly 16 SCLKs per CS assertion)
    static constexpr uint16_t SPI_OP =
        SPI_OP_MODE_MASTER |
        SPI_MODE_CPHA |
        SPI_WORD_SET(8) |
        SPI_TRANSFER_MSB;

    // SPI device handle
    const struct device* spi_dev_;

    // Chip-select GPIO, pulled from the &spi1 controller node's
    // existing "cs-gpios" property (see nucleo_f446re.overlay).
    // Driven MANUALLY around each spi_transceive() call, exactly as in
    // the verified standalone test (spi_cfg_.cs is intentionally left
    // unset -- we do not use Zephyr's automatic GPIO-CS handling here).
    struct gpio_dt_spec cs_gpio_;

    // SPI configuration
    struct spi_config spi_cfg_;
#else
    spi_config spi_cfg_{ .frequency = 4000000, .operation = 0x01 };
#endif

    IFilter* filter_ = nullptr;
    int last_error_ = 0;
    std::vector<int16_t> history_;
};