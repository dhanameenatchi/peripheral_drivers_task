// =============================================================================
// adc_driver.cpp — AdcDriver strategy filter and spi hal operations
// =============================================================================
#include "adc_driver.hpp"
#include <span>
#include <cerrno>

#ifdef ZEPHYR_BUILD
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(spi_app);
#endif

#ifdef ZEPHYR_BUILD
AdcDriver::AdcDriver(IFilter* filter)
    : spi_dev_(DEVICE_DT_GET(DT_NODELABEL(spi1)))
    , cs_gpio_(GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(spi1), cs_gpios, 0))
    , spi_cfg_{
          .frequency = 500000,   // 1 MHz -- exact value used by the verified standalone test
          .operation = SPI_OP,
          .slave = 0,
          // .cs intentionally left unset: CS is driven manually below,
          // exactly matching the standalone test's ads1118_transfer().
      }
    , filter_(filter)
{
    if (!device_is_ready(spi_dev_)) {
        LOG_ERR("SPI1 device not ready");
        last_error_ = -ENODEV;
        return;
    }
    if (!gpio_is_ready_dt(&cs_gpio_)) {
        LOG_ERR("ADS1118 CS GPIO not ready");
        last_error_ = -ENODEV;
        return;
    }
    // Configure CS as output, idling HIGH (inactive) -- identical to
    // spi_setup() in the verified standalone test.
    int ret = gpio_pin_configure_dt(&cs_gpio_, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        LOG_ERR("Failed to configure ADS1118 CS GPIO (err=%d)", ret);
        last_error_ = ret;
    }
}
#else
AdcDriver::AdcDriver(IFilter* filter) : filter_(filter) {}
#endif

void AdcDriver::setFilter(IFilter* f) {
    filter_ = f;
}

int16_t AdcDriver::readChannel(Channel ch, PGA pga, Mode mode) {
    // uint16_t cfg = static_cast<uint16_t>(ch) | static_cast<uint16_t>(pga) | 0x818B;
    // uint16_t cfg = 0xC483;
    uint16_t cfg = static_cast<uint16_t>(StartConv::START)       | // Bit 15
                   static_cast<uint16_t>(ch)                     | // Bits 14:12
                   static_cast<uint16_t>(pga)                    | // Bits 11:9
                   static_cast<uint16_t>(mode)                   | // Bit 8
                   static_cast<uint16_t>(DataRate::SPS_128)      | // Bits 7:5
                   static_cast<uint16_t>(SensorMode::ADC_MODE)   | // Bit 4
                   static_cast<uint16_t>(PullUp::DISABLE)         | // Bit 3
                   static_cast<uint16_t>(Nop::VALID)             | // Bits 2:1
                   RESERVED_BIT;                                   // Bit 0
    uint8_t tx[2] = {
        static_cast<uint8_t>(cfg >> 8),   // MSB
        static_cast<uint8_t>(cfg & 0xFF),  // LSB
    };
    uint8_t rx[2] = {0};

    struct spi_buf tx_buf = { .buf = tx, .len = 2 };
    struct spi_buf rx_buf = { .buf = rx, .len = 2 };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

#ifdef ZEPHYR_BUILD
    gpio_pin_set_dt(&cs_gpio_, 1); // Assert CS (Low)

    int err = spi_transceive(spi_dev_, &spi_cfg_, &tx_set, &rx_set);

    gpio_pin_set_dt(&cs_gpio_, 0); // Deassert CS (High)
#else
    int err = spi_transceive(nullptr, &spi_cfg_, &tx_set, &rx_set);
#endif

    if (err != 0) {
        last_error_ = err;
        return INT16_MIN;
    }

    // With SPI_WORD_SET(8), rx[0] is the MSB and rx[1] is the LSB
    int16_t raw = static_cast<int16_t>((static_cast<uint16_t>(rx[0]) << 8) | rx[1]);
    
    // Append to history and return
    history_.push_back(raw);
    return raw;
}

// int16_t AdcDriver::readChannel(Channel ch, PGA pga) {
//     // ADS1118 16-bit config word:
//     // Bit 15:     SS=1       (start single-shot)
//     // Bits 14:12: MUX        (from Channel enum)
//     // Bits 11:9:  PGA        (from PGA enum)
//     // Bit 8:      MODE=1     (single-shot)
//     // Bits 7:5:   DR=100     (128 SPS)
//     // Bit 4:      TS_MODE=0  (ADC mode)
//     // Bit 3:      PULL_UP=1  (DOUT pull-up enabled)
//     // Bits 2:1:   NOP=01     (valid config write)
//     // Bit 0:      reserved   (must be written as 1 per datasheet)
//     uint16_t cfg = static_cast<uint16_t>(ch) |
//                static_cast<uint16_t>(pga) |
//                0x818B;  // SS|MODE|DR=128SPS|PULL_UP|NOP=01|RESERVED=1
//             //  0xC38B;

//     // Exactly 16 SCLKs per CS assertion -- the ADS1118 frame is 2 bytes,
//     // not 4. Clocking extra bytes within the same CS-low period is
//     // outside the datasheet-specified protocol.
//     uint8_t tx[2] = {
//         static_cast<uint8_t>(cfg >> 8),
//         static_cast<uint8_t>(cfg & 0xFF),
//     };
//     uint8_t rx[2] = {};

//     spi_buf tx_buf{ tx, 2 };
//     spi_buf rx_buf{ rx, 2 };
//     spi_buf_set tx_set{ &tx_buf, 1 };
//     spi_buf_set rx_set{ &rx_buf, 1 };

// #ifdef ZEPHYR_BUILD
// LOG_INF("CFG = 0x%04X", cfg);
//     LOG_INF("SPI TX = %02X %02X", tx[0], tx[1]);

//     // Manual CS assert/deassert -- exact sequence used by the verified
//     // standalone test's ads1118_transfer(): assert before the transfer,
//     // deassert immediately after, regardless of success/failure.
//     gpio_pin_set_dt(&cs_gpio_, 1);   // assert (active-low -> logical 1 = asserted)
//     int err = spi_transceive(spi_dev_, &spi_cfg_, &tx_set, &rx_set);
//     gpio_pin_set_dt(&cs_gpio_, 0);   // deassert

//     LOG_INF("SPI err = %d", err);
//     LOG_INF("SPI RX = %02X %02X", rx[0], rx[1]);
//     uint16_t normal =
//     (static_cast<uint16_t>(rx[0]) << 8) | rx[1];

// uint16_t swapped =
//     (static_cast<uint16_t>(rx[1]) << 8) | rx[0];

// LOG_INF("Normal hex    = 0x%04X", normal);
// LOG_INF("Normal signed = %d", (int16_t)normal);

// LOG_INF("Swapped hex   = 0x%04X", swapped);
// LOG_INF("Swapped signed= %d", (int16_t)swapped);
// #else
//     int err = spi_transceive(nullptr, &spi_cfg_, &tx_set, &rx_set);
// #endif
//     if (err != 0) {
//         last_error_ = err;
//         return INT16_MIN;
//     }

//     // NOTE: For SPI_WORD_SET(16), the STM32 SPI driver packs the received
//     // 16-bit word into the byte buffer in the CPU's native (little-endian)
//     // order, not wire-transmission order. rx[0] is the LOW byte, rx[1] is
//     // the HIGH byte -- SPI_TRANSFER_MSB only governs bit order on the wire,
//     // not the resulting in-memory byte layout.
//     int16_t raw = static_cast<int16_t>(
//         (static_cast<uint16_t>(rx[1]) << 8) | rx[0]);
//     history_.push_back(raw);
//     return raw;
// }

int16_t AdcDriver::filteredValue() {
    if (!filter_ || history_.empty()) {
        return history_.empty() ? 0 : history_.back();
    }
    return filter_->apply(std::span<const int16_t>(history_));
}

int AdcDriver::lastError() const {
    return last_error_;
}

void AdcDriver::clearHistory() {
    history_.clear();
}

const std::vector<int16_t>& AdcDriver::history() const {
    return history_;
}