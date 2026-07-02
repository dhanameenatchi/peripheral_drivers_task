// =============================================================================
// adc_driver.cpp — AdcDriver strategy filter and spi hal operations
// =============================================================================
#include "adc_driver.hpp"
#include <span>

AdcDriver::AdcDriver(IFilter* filter) : filter_(filter) {}

void AdcDriver::setFilter(IFilter* f) {
    filter_ = f;
}

int16_t AdcDriver::readChannel(Channel ch, PGA pga) {
    uint16_t cfg = static_cast<uint16_t>(ch) |
                   static_cast<uint16_t>(pga) |
                   0x8100;  // OS=start, DR=128SPS, mode=single-shot

    uint8_t tx[4] = {
        static_cast<uint8_t>(cfg >> 8),
        static_cast<uint8_t>(cfg & 0xFF),
        0x00, 0x00
    };
    uint8_t rx[4] = {};

    spi_buf tx_buf{ tx, 4 };
    spi_buf rx_buf{ rx, 4 };
    spi_buf_set tx_set{ &tx_buf, 1 };
    spi_buf_set rx_set{ &rx_buf, 1 };

    int err = spi_transceive(nullptr, &spi_cfg_, &tx_set, &rx_set);
    if (err != 0) {
        last_error_ = err;
        return INT16_MIN;
    }

    int16_t raw = static_cast<int16_t>(
        (static_cast<uint16_t>(rx[0]) << 8) | rx[1]);
    history_.push_back(raw);
    return raw;
}

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
