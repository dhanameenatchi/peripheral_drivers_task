#pragma once
// =============================================================================
// SPI — ADS1118 ADC Driver + Strategy Pattern (IFilter)
// C++20, Zephyr SPI HAL wrapper
// Strategy swappable at runtime via UART command
// =============================================================================
#ifdef ZEPHYR_BUILD
  #include <zephyr/drivers/spi.h>
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
    enum class Channel : uint16_t {
        SE_AIN0 = 0x4000,  // AIN0 vs GND
        SE_AIN1 = 0x5000,  // AIN1 vs GND
        SE_AIN2 = 0x6000,  // AIN2 vs GND
        SE_AIN3 = 0x7000,  // AIN3 vs GND
        DIFF_01 = 0x0000,  // AIN0 - AIN1
        DIFF_23 = 0x3000,  // AIN2 - AIN3
    };
    enum class PGA : uint16_t {
        FSR_6V  = 0x0000,
        FSR_4V  = 0x0200,
        FSR_2V  = 0x0400,  // default
        FSR_1V  = 0x0600,
    };

    explicit AdcDriver(IFilter* filter = nullptr);

    void setFilter(IFilter* f);

    // Read raw ADC value from selected channel
    int16_t readChannel(Channel ch, PGA pga = PGA::FSR_2V);

    // Apply current strategy filter to history
    int16_t filteredValue();

    [[nodiscard]] int  lastError() const;
    void clearHistory();
    [[nodiscard]] const std::vector<int16_t>& history() const;

private:
    spi_config spi_cfg_{ .frequency = 4000000, .operation = 0x01 }; // Mode 1
    IFilter*              filter_     = nullptr;
    int                   last_error_ = 0;
    std::vector<int16_t>  history_;   // test builds use vector; embedded: static array
};
