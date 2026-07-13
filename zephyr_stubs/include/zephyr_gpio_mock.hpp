// #pragma once
// // =============================================================================
// // Zephyr RTOS HAL Stubs — host-side unit-test shim
// // All ISR / DMA / I2C / SPI / GPIO kernel calls are replaced with lightweight
// // test doubles so GTest can run on the developer's machine without hardware.
// // =============================================================================
// #include <cstdint>
// #include <cstring>
// #include <functional>
// #include <vector>

// // ---------------------------------------------------------------------------
// // Kernel primitives
// // ---------------------------------------------------------------------------
// using k_timeout_t = int64_t;
// #define K_NO_WAIT   0
// #define K_FOREVER  -1
// #define K_MSEC(ms) (ms)

// struct k_timer {
//     std::function<void(k_timer*)> expiry_fn;
//     bool running = false;
// };

// inline void k_timer_init(k_timer* t, void (*fn)(k_timer*), void*) {
//     t->expiry_fn = fn;
// }
// inline void k_timer_start(k_timer* t, k_timeout_t, k_timeout_t) { t->running = true; }
// inline void k_timer_stop(k_timer* t) { t->running = false; }
// // Test helper: fire the timer manually
// inline void k_timer_fire(k_timer* t) { if (t->expiry_fn) t->expiry_fn(t); }

// inline uint32_t k_uptime_get_32() {
//     static uint32_t fake_ms = 0;
//     return fake_ms += 10;
// }

// // ---------------------------------------------------------------------------
// // GPIO stubs
// // ---------------------------------------------------------------------------
// #define GPIO_INPUT   0x01
// #define GPIO_OUTPUT  0x02
// #define GPIO_INT_EDGE_BOTH 0x10

// struct gpio_dt_spec {
//     int port;
//     int pin;
//     int dt_flags;
// };

// // Simulated pin state bank (256 pins max)
// namespace gpio_sim {
//     inline bool pin_state[256] = {};
//     inline bool configure_ok = true;
// }

// inline int gpio_pin_configure_dt(const gpio_dt_spec* spec, int) {
//     return gpio_sim::configure_ok ? 0 : -EIO;
// }
// inline int gpio_pin_get_dt(const gpio_dt_spec* spec) {
//     return gpio_sim::pin_state[spec->pin] ? 1 : 0;
// }
// inline int gpio_pin_toggle_dt(const gpio_dt_spec* spec) {
//     gpio_sim::pin_state[spec->pin] = !gpio_sim::pin_state[spec->pin];
//     return 0;
// }
// inline int gpio_pin_set_dt(const gpio_dt_spec* spec, int val) {
//     gpio_sim::pin_state[spec->pin] = (val != 0);
//     return 0;
// }

// struct gpio_callback { int pin; std::function<void()> fn; };
// inline int gpio_add_callback(int, gpio_callback*) { return 0; }
// inline int gpio_pin_interrupt_configure_dt(const gpio_dt_spec*, int) { return 0; }

// // ---------------------------------------------------------------------------
// // I2C stubs
// // ---------------------------------------------------------------------------
// struct i2c_msg { uint8_t* buf; uint32_t len; uint8_t flags; };
// #define I2C_MSG_WRITE 0x00
// #define I2C_MSG_READ  0x01
// #define I2C_MSG_STOP  0x02

// namespace i2c_sim {
//     // Per-address register banks: addr -> reg -> value
//     inline uint8_t regs[128][256] = {};
//     inline bool nack_next = false;  // inject NACK error on next I2C op
//     // nack_after_n: when > 0, decrement on each I2C op; NACK when it reaches 0
//     // Set to 2 to allow first read to succeed and fail on second read.
//     inline int nack_after_n = -1;   // -1 = disabled
// }

// inline int i2c_write_read_dt(const void* dev, const uint8_t* wbuf, size_t wlen,
//                               uint8_t* rbuf, size_t rlen)
// {
//     if (i2c_sim::nack_next) { i2c_sim::nack_next = false; return -EIO; }
//     if (i2c_sim::nack_after_n >= 0) {
//         if (i2c_sim::nack_after_n == 0) { i2c_sim::nack_after_n = -1; return -EIO; }
//         --i2c_sim::nack_after_n;
//     }
//     // Treat first write byte as register address
//     if (wlen >= 1) {
//         uint8_t reg = wbuf[0];
//         // dev pointer encodes address as uintptr_t for test purposes
//         uint8_t addr = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(dev) & 0x7F);
//         for (size_t i = 0; i < rlen; ++i)
//             rbuf[i] = i2c_sim::regs[addr][(reg + i) & 0xFF];
//     }
//     return 0;
// }

// inline int i2c_write_dt(const void* dev, const uint8_t* buf, size_t len) {
//     if (i2c_sim::nack_next) { i2c_sim::nack_next = false; return -EIO; }
//     if (len >= 2) {
//         uint8_t addr = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(dev) & 0x7F);
//         i2c_sim::regs[addr][buf[0]] = buf[1];
//     }
//     return 0;
// }

// // ---------------------------------------------------------------------------
// // SPI stubs
// // ---------------------------------------------------------------------------
// struct spi_config { uint32_t frequency; uint16_t operation; };
// struct spi_buf { void* buf; size_t len; };
// struct spi_buf_set { const spi_buf* buffers; size_t count; };

// namespace spi_sim {
//     inline std::vector<uint8_t> tx_capture;
//     inline std::vector<uint8_t> rx_inject;
//     inline bool error_next = false;
// }

// inline int spi_transceive(const void*, const spi_config*,
//                            const spi_buf_set* tx, const spi_buf_set* rx)
// {
//     if (spi_sim::error_next) { spi_sim::error_next = false; return -EIO; }
//     if (tx && tx->count > 0) {
//         auto* b = reinterpret_cast<const uint8_t*>(tx->buffers[0].buf);
//         spi_sim::tx_capture.insert(spi_sim::tx_capture.end(), b, b + tx->buffers[0].len);
//     }
//     if (rx && rx->count > 0 && !spi_sim::rx_inject.empty()) {
//         size_t n = std::min(rx->buffers[0].len, spi_sim::rx_inject.size());
//         std::memcpy(rx->buffers[0].buf, spi_sim::rx_inject.data(), n);
//     }
//     return 0;
// }

// // ---------------------------------------------------------------------------
// // DMA / UART stubs
// // ---------------------------------------------------------------------------
// namespace uart_sim {
//     inline std::vector<uint8_t> tx_buf;
//     inline bool dma_error = false;
// }

// // errno codes
// #ifndef EIO
// #define EIO 5
// #endif

#pragma once
// =============================================================================
// zephyr_mock.hpp — Host (GTest) stubs for Zephyr HAL types and functions
// Used when ZEPHYR_BUILD is NOT defined (i.e. native Linux GTest build).
// =============================================================================
#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Fix 1: gpio_dt_spec — port must be `const device*` (pointer), not int.
// The real Zephyr struct is:
//   struct gpio_dt_spec { const struct device *port; gpio_pin_t pin; ... };
// Using int caused: "cannot convert 'std::nullptr_t' to 'int'"
// in the constexpr pin definitions in digital_io.hpp.
// ---------------------------------------------------------------------------
struct device {
    // opaque in stubs — never dereferenced on host
};

using gpio_pin_t   = uint8_t;
using gpio_flags_t = uint32_t;

struct gpio_dt_spec {
    const struct device* port;   // ← was int; must be pointer to accept nullptr
    gpio_pin_t           pin;
    gpio_flags_t         dt_flags;
};

// ---------------------------------------------------------------------------
// GPIO config flags (subset used by our driver)
// ---------------------------------------------------------------------------
static constexpr gpio_flags_t GPIO_INPUT    = (1u << 0);
static constexpr gpio_flags_t GPIO_OUTPUT   = (1u << 1);
static constexpr gpio_flags_t GPIO_PULL_UP  = (1u << 4);
static constexpr gpio_flags_t GPIO_INT_EDGE_BOTH = (1u << 8);

// ---------------------------------------------------------------------------
// GPIO HAL stubs — no-ops on host; tests verify behaviour via notify() directly
// ---------------------------------------------------------------------------
namespace gpio_sim {
    inline bool pin_state[256] = {};
    inline bool configure_ok = true;
}

inline int  gpio_pin_configure_dt(const gpio_dt_spec*, gpio_flags_t) {
    return gpio_sim::configure_ok ? 0 : -5;
}
inline int  gpio_pin_get_dt(const gpio_dt_spec* spec) {
    return gpio_sim::pin_state[spec->pin] ? 1 : 0;
}
inline int  gpio_pin_set_dt(const gpio_dt_spec* spec, int val) {
    gpio_sim::pin_state[spec->pin] = (val != 0);
    return 0;
}
inline int  gpio_pin_toggle_dt(const gpio_dt_spec* spec) {
    gpio_sim::pin_state[spec->pin] = !gpio_sim::pin_state[spec->pin];
    return 0;
}
inline bool gpio_is_ready_dt(const gpio_dt_spec*)                      { return true; }

// ---------------------------------------------------------------------------
// k_work stub
// ---------------------------------------------------------------------------
struct k_work {
    void (*handler)(k_work*) = nullptr;
};

inline void k_work_init(k_work* w, void (*handler)(k_work*)) {
    w->handler = handler;
}

inline int k_work_submit(k_work* w) {
    if (w && w->handler) {
        w->handler(w);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// k_timer stub
// Stores the expiry callback so fireDebounceTimer() can invoke it directly.
// ---------------------------------------------------------------------------
struct k_timer {
    void (*expiry_fn)(k_timer*) = nullptr;
    void* user_data             = nullptr;
};

inline void k_timer_init(k_timer* t,
                         void (*expiry)(k_timer*),
                         void (*stop)(k_timer*))
{
    (void)stop;
    t->expiry_fn = expiry;
}

inline void k_timer_start(k_timer* t, uint32_t /*duration_ms*/, uint32_t /*period_ms*/)
{
    (void)t;   // on host: timer does not auto-fire; test calls fireDebounceTimer()
}

inline void k_timer_stop(k_timer* t) { (void)t; }

// k_timer_fire: test helper — immediately invokes the stored expiry callback.
// This is what gpio_test.cpp calls via bus.fireDebounceTimer().
inline void k_timer_fire(k_timer* t)
{
    if (t && t->expiry_fn) {
        t->expiry_fn(t);
    }
}

// ---------------------------------------------------------------------------
// Timing stub
// ---------------------------------------------------------------------------
inline uint32_t k_uptime_get_32() { return 0; }

// K_MSEC / K_NO_WAIT — used in onRawInterrupt(); treated as plain integers on host
#define K_MSEC(ms)   (static_cast<uint32_t>(ms))
#define K_NO_WAIT    (static_cast<uint32_t>(0))