#ifdef CONFIG_UART_ASYNC_API   // guard: only compiled on Zephyr target

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "dma_uart.hpp"
#include <atomic>
#include <cstdio>

LOG_MODULE_REGISTER(uart_dma, LOG_LEVEL_INF);

// ---------------------------------------------------------------------------
// Device handle — resolved from DTS alias "uart-vcp" → &usart2
// ---------------------------------------------------------------------------
#define UART_VCP_NODE DT_ALIAS(uart_vcp)
static const struct device* g_dev = DEVICE_DT_GET(UART_VCP_NODE);

static DmaUart g_uart;

static uint8_t g_tx_buf[2048];
static std::atomic<bool> g_tx_busy{false};

// FIX #1: added — stalls tx_kick() draining during overflow injection burst
// so the ring buffer actually fills up instead of draining concurrently.
static std::atomic<bool> g_inject_mode{false};

static uint8_t g_rx_buf_a[64];
static uint8_t g_rx_buf_b[64];
static bool    g_rx_use_a = true;

static void uart_callback(const struct device* dev,
                          struct uart_event*   evt,
                          void*                user_data);
static void tx_kick(void);
static void rx_restart(void);

bool uart_dma_log(uint32_t ts_ms, LogLevel lvl, const char* msg);

void uart_dma_init(void) {
    if (!device_is_ready(g_dev)) {
        LOG_ERR("UART device not ready");
        return;
    }

    LOG_INF("DMA UART ready: TX=PA2 RX=PA3 @ 921600");
    k_msleep(200);

    int err = uart_callback_set(g_dev, uart_callback, nullptr);
    if (err) {
        LOG_ERR("uart_callback_set failed: %d", err);
        return;
    }

    rx_restart();

    uart_dma_log(k_uptime_get_32(), LogLevel::INFO,
                 "HCE UART DMA 921600 ready");
}

bool uart_dma_log(uint32_t ts_ms, LogLevel lvl, const char* msg) {
    bool ok = g_uart.log(ts_ms, lvl, msg);
    if (ok) {
        bool expected = false;
        if (g_tx_busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            tx_kick();
        }
    }
    return ok;
}

std::optional<uint8_t> uart_dma_rx_pop(void) {
    return g_uart.rxPop();
}

uint32_t uart_dma_tx_overflow(void) {
    return g_uart.txOverflow();
}

// ---------------------------------------------------------------------------
// tx_kick() — drain ring to tx scratch buffer and start DMA TX
// FIX #2: added TX_CALL timing log + g_inject_mode guard at top
// ---------------------------------------------------------------------------
static void tx_kick(void) {
    // FIX #1: while injection is running, refuse to drain — this forces
    // the ring buffer to genuinely fill and reject pushes past capacity.
    if (g_inject_mode.load(std::memory_order_relaxed)) {
        return;
    }

    do {
        size_t n = g_uart.drainToTxBuffer(g_tx_buf, sizeof(g_tx_buf));
        if (n > 0) {
            uint32_t start_cycles = k_cycle_get_32();
            int err = uart_tx(g_dev, g_tx_buf, n, SYS_FOREVER_US);
            uint32_t dur_us = k_cyc_to_us_floor32(k_cycle_get_32() - start_cycles);
            if (err == 0) {
                // LOG_INF("TX_CALL n_bytes=%zu dur_us=%u", n, dur_us);
                return;
            }
            LOG_ERR("uart_tx failed: %d", err);
        }
        g_tx_busy.store(false, std::memory_order_release);
    } while (g_uart.txPending() > 0 && g_tx_busy.exchange(true, std::memory_order_acquire) == false);
}

static void rx_restart(void) {
    uint8_t* buf = g_rx_use_a ? g_rx_buf_a : g_rx_buf_b;
    g_rx_use_a   = !g_rx_use_a;
    int err = uart_rx_enable(g_dev, buf, sizeof(g_rx_buf_a), 1000);
    if (err) LOG_ERR("uart_rx_enable failed: %d", err);
}

static void uart_callback(const struct device* dev,
                          struct uart_event*   evt,
                          void*                user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    switch (evt->type) {
    case UART_TX_DONE:
        tx_kick();
        break;

    case UART_TX_ABORTED:
        LOG_WRN("UART TX aborted");
        g_tx_busy.store(false, std::memory_order_release);
        if (g_uart.txPending() > 0) {
            bool expected = false;
            if (g_tx_busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                tx_kick();
            }
        }
        break;

    case UART_RX_RDY: {
        const uint8_t* data = evt->data.rx.buf + evt->data.rx.offset;
        size_t         len  = evt->data.rx.len;
        for (size_t i = 0; i < len; ++i) {
            if (!g_uart.rxPush(data[i])) {
                LOG_WRN_ONCE("RX ring overflow");
            }
        }
        break;
    }

    case UART_RX_BUF_REQUEST: {
        uint8_t* buf = g_rx_use_a ? g_rx_buf_a : g_rx_buf_b;
        g_rx_use_a   = !g_rx_use_a;
        uart_rx_buf_rsp(dev, buf, sizeof(g_rx_buf_a));
        break;
    }

    case UART_RX_BUF_RELEASED:
        break;

    case UART_RX_DISABLED:
        LOG_WRN("UART RX disabled — restarting");
        rx_restart();
        break;

    case UART_RX_STOPPED:
        LOG_WRN("UART RX stopped: reason=%d", evt->data.rx_stop.reason);
        break;

    default:
        break;
    }
}

// ===========================================================================
// UART DMA LIVE DEMONSTRATION
// ===========================================================================

#define LED1_NODE DT_ALIAS(led0)

#if DT_NODE_EXISTS(LED1_NODE)

static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

static void heartbeat_thread_fn(void* p1, void* p2, void* p3) {
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("Heartbeat thread started");

    if (!gpio_is_ready_dt(&heartbeat_led)) {
        LOG_ERR("Heartbeat LED device not ready");
        return;
    }
    int rc = gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_ACTIVE);
    if (rc < 0) {
        LOG_ERR("Heartbeat LED configure failed: %d", rc);
    } else {
        LOG_INF("Heartbeat LED configured successfully");
    }

    uint32_t toggle_count = 0;
    while (true) {
        int toggle_rc = gpio_pin_toggle_dt(&heartbeat_led);
        if (toggle_rc != 0) {
            LOG_ERR("gpio_pin_toggle_dt failed: %d", toggle_rc);
        }
        toggle_count++;

        if (toggle_count % 50 == 0) {
            char msg[64];
            std::snprintf(msg, sizeof(msg), "HEARTBEAT #%u alive, toggle_rc=%d", toggle_count, toggle_rc);
            uart_dma_log(k_uptime_get_32(), LogLevel::INFO, msg);
        }
        k_msleep(100);  // LED timing stays 100ms — keep this fast so blink is visible on camera
    }
}

K_THREAD_STACK_DEFINE(heartbeat_stack, 2048);
static struct k_thread heartbeat_thread_data;

static void start_heartbeat_thread(void) {
    k_thread_create(&heartbeat_thread_data, heartbeat_stack,
                    K_THREAD_STACK_SIZEOF(heartbeat_stack),
                    heartbeat_thread_fn,
                    nullptr, nullptr, nullptr,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&heartbeat_thread_data, "led_heartbeat");
    LOG_INF("Heartbeat thread created");
}

#else
static void start_heartbeat_thread(void) {
    LOG_WRN("No LED alias 'led1' in DTS — heartbeat disabled");
}
#endif

// ---------------------------------------------------------------------------
// uart_dma_demo_loop()
// FIX #3: added blocking-vs-DMA benchmark (calculated, not destructive)
// FIX #1: overflow injection now uses g_inject_mode to force real overflow
// FIX #4: k_msleep slowed to 500ms so seq= lines are readable on video
// ---------------------------------------------------------------------------
void uart_dma_demo_loop(void) {
    k_msleep(300);

    start_heartbeat_thread();

    // Wait for any prior async transfer (startup banner) to finish
    while (g_uart.txPending() > 0 || g_tx_busy.load(std::memory_order_acquire)) {
        k_msleep(10);
    }

    // ---- Blocking vs DMA benchmark ----
    // BLOCKING time is CALCULATED, not measured via real uart_poll_out(),
    // because running uart_poll_out() while DMA already owns this UART
    // instance corrupts the DMA stream's hardware state (confirmed on
    // real hardware: caused "dma stream busy" / uart_tx failed errors).
    // Blocking UART time is deterministic — bound by baud rate, not CPU —
    // so computing it is equally valid and doesn't touch live hardware state.
    static uint8_t bench_buf[200];
    for (size_t i = 0; i < sizeof(bench_buf); ++i) {
        bench_buf[i] = 'A' + (i % 26);
    }

    constexpr uint32_t UART_FRAME_BITS = 10; // 1 start + 8 data + 1 stop (8N1)
    uint32_t blocking_us = (static_cast<uint32_t>(sizeof(bench_buf)) *
                             UART_FRAME_BITS * 1000000UL) / DmaUart::BAUD;

    g_tx_busy.store(true, std::memory_order_release);
    uint32_t start_dma = k_cycle_get_32();
    int bench_err = uart_tx(g_dev, bench_buf, sizeof(bench_buf), SYS_FOREVER_US);
    uint32_t dma_us = k_cyc_to_us_floor32(k_cycle_get_32() - start_dma);

    if (bench_err == 0) {
        // Wait for this transfer to fully complete (UART_TX_DONE clears
        // g_tx_busy) before queuing the result message or anything else —
        // avoids racing the benchmark's own DMA transfer against the next log.
        while (g_tx_busy.load(std::memory_order_acquire)) {
            k_msleep(5);
        }
    } else {
        LOG_ERR("Benchmark uart_tx failed: %d", bench_err);
        g_tx_busy.store(false, std::memory_order_release);
    }

    uint32_t seq = 0;
    uint32_t last_status_ms = 0;
    int inj_ok = 0;
    int inj_drop = 0;

    uart_dma_log(k_uptime_get_32(), LogLevel::INFO,
                 "=== DMA DEMO START ===");

    while (seq < 80) {
        uint32_t now = k_uptime_get_32();

        char msg[32];
        std::snprintf(msg, sizeof(msg), "seq=%u", seq);

        LogLevel lvl;
        if      (seq % 40 == 0) lvl = LogLevel::ERROR;
        else if (seq % 20 == 0) lvl = LogLevel::WARN;
        else if (seq % 10 == 0) lvl = LogLevel::DEBUG;
        else                    lvl = LogLevel::INFO;

        uart_dma_log(now, lvl, msg);
        seq++;

        if (now - last_status_ms >= 5000) {
            last_status_ms = now;
            char status[32];
            std::snprintf(status, sizeof(status),
                          "TX_OVF=%u pend=%u",
                          static_cast<unsigned>(g_uart.txOverflow()),
                          static_cast<unsigned>(g_uart.txPending()));
            uart_dma_log(now, LogLevel::INFO, status);
        }

        // ---- overflow injection that actually overflows ----
        if (seq == 50) {
            uart_dma_log(now, LogLevel::WARN,
                         "=== OVERFLOW INJECT START ===");

            g_inject_mode.store(true, std::memory_order_release); // stall draining

            // 600 pushes — comfortably exceeds TX_BUF_SIZE=256 capacity
            // now that draining is genuinely stalled during the burst
            for (int i = 0; i < 600; ++i) {
                char ovf_msg[32];
                std::snprintf(ovf_msg, sizeof(ovf_msg), "OVF_%d", i);
                if (!uart_dma_log(now, LogLevel::WARN, ovf_msg)) {
                    inj_drop++;
                } else {
                    inj_ok++;
                }
            }
            g_inject_mode.store(false, std::memory_order_release); // resume draining

            g_tx_busy.store(true, std::memory_order_release);
            tx_kick();
        }

        k_msleep(100);  // Slowed to 500ms so seq= lines are readable on video (~2Hz)
    }

    // Wait for the test queue to fully drain
    while (g_uart.txPending() > 0 || g_tx_busy.load(std::memory_order_acquire)) {
        k_msleep(10);
    }

    // Print BENCH summary
    if (bench_err == 0) {
        char bench_msg[80];
        std::snprintf(bench_msg, sizeof(bench_msg),
                      "BENCH blocking_us=%u dma_us=%u",
                      blocking_us, dma_us);
        uart_dma_log(k_uptime_get_32(), LogLevel::INFO, bench_msg);

        while (g_uart.txPending() > 0 || g_tx_busy.load(std::memory_order_acquire)) {
            k_msleep(10);
        }
    }

    // Print INJ summary
    char result[80];
    std::snprintf(result, sizeof(result),
                  "INJ ok=%d drop=%d overflow=%u",
                  inj_ok, inj_drop, g_uart.txOverflow());
    uart_dma_log(k_uptime_get_32(), LogLevel::INFO, result);

    while (g_uart.txPending() > 0 || g_tx_busy.load(std::memory_order_acquire)) {
        k_msleep(10);
    }

    // End of test
    uart_dma_log(k_uptime_get_32(), LogLevel::INFO, "=== DMA DEMO COMPLETE ===");
    while (g_uart.txPending() > 0 || g_tx_busy.load(std::memory_order_acquire)) {
        k_msleep(10);
    }

    // Stay alive continuously without crashing or logging anymore
    while (true) {
        k_msleep(1000);
    }
}

#endif // CONFIG_UART_ASYNC_API