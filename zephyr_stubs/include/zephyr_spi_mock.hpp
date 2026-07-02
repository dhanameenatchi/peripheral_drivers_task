#pragma once
// =============================================================================
// zephyr_spi_mock.hpp — SPI host-side unit-test stubs
// =============================================================================
#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm>

struct device {
    // opaque in stubs
};

struct spi_config {
    uint32_t frequency;
    uint16_t operation;
};

struct spi_buf {
    void* buf;
    size_t len;
};

struct spi_buf_set {
    const struct spi_buf* buffers;
    size_t count;
};

namespace spi_sim {
    inline std::vector<uint8_t> tx_capture;
    inline std::vector<uint8_t> rx_inject;
    inline bool error_next = false;

    inline void reset() {
        tx_capture.clear();
        rx_inject.clear();
        error_next = false;
    }

    inline void clear() {
        tx_capture.clear();
        rx_inject.clear();
    }

    inline void injectRx(const std::vector<uint8_t>& data) {
        rx_inject = data;
    }
}

inline int spi_transceive(const struct device* dev, const struct spi_config* config,
                          const struct spi_buf_set* tx_bufs, const struct spi_buf_set* rx_bufs)
{
    (void)dev;
    (void)config;

    if (spi_sim::error_next) {
        spi_sim::error_next = false;
        return -5; // negative error code
    }

    // Capture tx bytes
    if (tx_bufs && tx_bufs->buffers) {
        for (size_t i = 0; i < tx_bufs->count; ++i) {
            const auto& b = tx_bufs->buffers[i];
            if (b.buf && b.len > 0) {
                const uint8_t* p = reinterpret_cast<const uint8_t*>(b.buf);
                spi_sim::tx_capture.insert(spi_sim::tx_capture.end(), p, p + b.len);
            }
        }
    }

    // Copy rx_inject into rx_bufs
    if (rx_bufs && rx_bufs->buffers) {
        size_t rx_idx = 0;
        for (size_t i = 0; i < rx_bufs->count; ++i) {
            const auto& b = rx_bufs->buffers[i];
            if (b.buf && b.len > 0) {
                uint8_t* p = reinterpret_cast<uint8_t*>(b.buf);
                for (size_t j = 0; j < b.len; ++j) {
                    if (rx_idx < spi_sim::rx_inject.size()) {
                        p[j] = spi_sim::rx_inject[rx_idx++];
                    } else {
                        p[j] = 0;
                    }
                }
            }
        }
    }

    return 0;
}
