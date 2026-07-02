#pragma once
// =============================================================================
// zephyr_uart_mock.hpp — UART host-side unit-test stubs
// Used when ZEPHYR_BUILD is NOT defined (i.e. native Linux GTest build).
// =============================================================================
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------------
// Fix: device — only define if not already defined (or opaque struct)
// ---------------------------------------------------------------------------
struct device {
    // opaque in stubs — never dereferenced on host
};

// ---------------------------------------------------------------------------
// Zephyr UART type definitions
// ---------------------------------------------------------------------------
enum uart_event_type {
    UART_TX_DONE,
    UART_TX_ABORTED,
    UART_RX_RDY,
    UART_RX_BUF_REQUEST,
    UART_RX_BUF_RELEASED,
    UART_RX_DISABLED,
    UART_RX_STOPPED
};

struct uart_event {
    uart_event_type type;
    union {
        struct {
            const uint8_t *buf;
            size_t len;
        } tx;
        struct {
            uint8_t *buf;
            size_t offset;
            size_t len;
        } rx;
        struct {
            int reason;
        } rx_stop;
    } data;
};

typedef void (*uart_callback_t)(const struct device *dev,
                                 struct uart_event *evt,
                                 void *user_data);

#ifndef SYS_FOREVER_US
#define SYS_FOREVER_US (-1)
#endif

// ---------------------------------------------------------------------------
// UART simulation state & helper APIs
// ---------------------------------------------------------------------------
namespace uart_sim {
    // Callback registered by uart_callback_set
    inline uart_callback_t callback = nullptr;
    inline void* callback_user_data = nullptr;

    // TX inspection
    inline uint8_t tx_buffer[4096] = {};
    inline size_t tx_size = 0;
    inline uint32_t tx_call_count = 0;

    // Mock behavior controls
    inline bool device_ready = true;
    inline int callback_set_result = 0;
    inline int tx_result = 0;
    inline int rx_enable_result = 0;
    inline int rx_buf_rsp_result = 0;

    // Buffer references to manage simulated RX buffers
    inline uint8_t* rx_buffer_active = nullptr;
    inline size_t rx_buffer_active_len = 0;
    inline uint8_t* rx_buffer_next = nullptr;
    inline size_t rx_buffer_next_len = 0;

    // Reset function
    inline void reset() {
        callback = nullptr;
        callback_user_data = nullptr;
        std::memset(tx_buffer, 0, sizeof(tx_buffer));
        tx_size = 0;
        tx_call_count = 0;
        device_ready = true;
        callback_set_result = 0;
        tx_result = 0;
        rx_enable_result = 0;
        rx_buf_rsp_result = 0;
        rx_buffer_active = nullptr;
        rx_buffer_active_len = 0;
        rx_buffer_next = nullptr;
        rx_buffer_next_len = 0;
    }

    // Getters for TX Inspection
    inline const uint8_t* get_tx_buffer() { return tx_buffer; }
    inline size_t get_tx_size() { return tx_size; }
    inline uint32_t get_tx_call_count() { return tx_call_count; }
}

// ---------------------------------------------------------------------------
// Zephyr UART HAL functions (Stubs)
// ---------------------------------------------------------------------------
inline bool device_is_ready(const struct device* dev) {
    (void)dev;
    return uart_sim::device_ready;
}

inline int uart_callback_set(const struct device* dev, uart_callback_t callback, void* user_data) {
    (void)dev;
    if (uart_sim::callback_set_result != 0) {
        return uart_sim::callback_set_result;
    }
    uart_sim::callback = callback;
    uart_sim::callback_user_data = user_data;
    return 0;
}

inline int uart_tx(const struct device* dev, const uint8_t* buf, size_t len, int32_t timeout) {
    (void)dev;
    (void)timeout;
    uart_sim::tx_call_count++;
    if (uart_sim::tx_result != 0) {
        return uart_sim::tx_result;
    }
    if (len > 0 && buf != nullptr) {
        size_t copy_len = std::min(len, sizeof(uart_sim::tx_buffer) - uart_sim::tx_size);
        std::memcpy(uart_sim::tx_buffer + uart_sim::tx_size, buf, copy_len);
        uart_sim::tx_size += copy_len;
    }
    return 0;
}

inline int uart_rx_enable(const struct device* dev, uint8_t* buf, size_t len, int32_t timeout) {
    (void)dev;
    (void)timeout;
    if (uart_sim::rx_enable_result != 0) {
        return uart_sim::rx_enable_result;
    }
    uart_sim::rx_buffer_active = buf;
    uart_sim::rx_buffer_active_len = len;
    return 0;
}

inline int uart_rx_buf_rsp(const struct device* dev, uint8_t* buf, size_t len) {
    (void)dev;
    if (uart_sim::rx_buf_rsp_result != 0) {
        return uart_sim::rx_buf_rsp_result;
    }
    uart_sim::rx_buffer_next = buf;
    uart_sim::rx_buffer_next_len = len;
    return 0;
}

// ---------------------------------------------------------------------------
// Sim/Mock Helper Functions (for unit tests)
// ---------------------------------------------------------------------------
namespace uart_sim {
    inline void register_callback(uart_callback_t cb, void* user_data) {
        callback = cb;
        callback_user_data = user_data;
    }

    inline void trigger_tx_done(const struct device* dev) {
        if (callback) {
            struct uart_event evt{};
            evt.type = UART_TX_DONE;
            evt.data.tx.buf = tx_buffer;
            evt.data.tx.len = tx_size;
            callback(dev, &evt, callback_user_data);
        }
    }

    inline void trigger_tx_aborted(const struct device* dev) {
        if (callback) {
            struct uart_event evt{};
            evt.type = UART_TX_ABORTED;
            evt.data.tx.buf = tx_buffer;
            evt.data.tx.len = tx_size;
            callback(dev, &evt, callback_user_data);
        }
    }

    inline void trigger_rx_rdy(const struct device* dev, uint8_t* buf, size_t offset, size_t len) {
        if (callback) {
            struct uart_event evt{};
            evt.type = UART_RX_RDY;
            evt.data.rx.buf = buf;
            evt.data.rx.offset = offset;
            evt.data.rx.len = len;
            callback(dev, &evt, callback_user_data);
        }
    }

    inline void trigger_rx_buf_request(const struct device* dev) {
        if (callback) {
            struct uart_event evt{};
            evt.type = UART_RX_BUF_REQUEST;
            callback(dev, &evt, callback_user_data);
        }
    }

    inline void trigger_rx_buf_released(const struct device* dev, uint8_t* buf) {
        if (callback) {
            struct uart_event evt{};
            evt.type = UART_RX_BUF_RELEASED;
            evt.data.rx.buf = buf;
            callback(dev, &evt, callback_user_data);
        }
    }

    inline void trigger_rx_disabled(const struct device* dev) {
        if (callback) {
            struct uart_event evt{};
            evt.type = UART_RX_DISABLED;
            callback(dev, &evt, callback_user_data);
        }
    }

    inline void trigger_rx_stopped(const struct device* dev, int reason) {
        if (callback) {
            struct uart_event evt{};
            evt.type = UART_RX_STOPPED;
            evt.data.rx_stop.reason = reason;
            callback(dev, &evt, callback_user_data);
        }
    }

    inline void inject_rx_bytes(const struct device* dev, const uint8_t* data, size_t len) {
        size_t processed = 0;
        while (processed < len) {
            if (!rx_buffer_active) {
                break;
            }
            size_t available = rx_buffer_active_len;
            size_t to_copy = std::min(len - processed, available);
            if (to_copy > 0) {
                std::memcpy(rx_buffer_active, data + processed, to_copy);
                trigger_rx_rdy(dev, rx_buffer_active, 0, to_copy);
                processed += to_copy;
            }

            if (processed < len || to_copy == available) {
                trigger_rx_buf_request(dev);
                if (rx_buffer_next) {
                    trigger_rx_buf_released(dev, rx_buffer_active);
                    rx_buffer_active = rx_buffer_next;
                    rx_buffer_active_len = rx_buffer_next_len;
                    rx_buffer_next = nullptr;
                    rx_buffer_next_len = 0;
                } else {
                    break;
                }
            }
        }
    }
}
