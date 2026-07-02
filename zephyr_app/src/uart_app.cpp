
#ifdef CONFIG_UART_ASYNC_API   // guard: only compiled on Zephyr target

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include "dma_uart.hpp"
#include <atomic>

LOG_MODULE_REGISTER(uart_dma, LOG_LEVEL_INF);

// ---------------------------------------------------------------------------
// Device handle — resolved from DTS alias "uart-vcp" → &usart2
// ---------------------------------------------------------------------------
#define UART_VCP_NODE DT_ALIAS(uart_vcp)
static const struct device* g_dev = DEVICE_DT_GET(UART_VCP_NODE);

// ---------------------------------------------------------------------------
// Global DmaUart ring-buffer instance (driver state lives here)
// ---------------------------------------------------------------------------
static DmaUart g_uart;

// ---------------------------------------------------------------------------
// TX scratch buffer — formatted text sent to UART DMA engine
// Size matches DmaUart::TX_BUF_SIZE formatted output worst-case
// ---------------------------------------------------------------------------
static uint8_t g_tx_buf[2048];
static std::atomic<bool> g_tx_busy{false};

// ---------------------------------------------------------------------------
// RX ping-pong buffers (double buffering for continuous DMA RX)
// ---------------------------------------------------------------------------
static uint8_t g_rx_buf_a[64];
static uint8_t g_rx_buf_b[64];
static bool    g_rx_use_a = true;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void uart_callback(const struct device* dev,
                          struct uart_event*   evt,
                          void*                user_data);
static void tx_kick(void);
static void rx_restart(void);

// ---------------------------------------------------------------------------
// uart_dma_init() — call once from application init
// ---------------------------------------------------------------------------
void uart_dma_init(void) {
    if (!device_is_ready(g_dev)) {
        LOG_ERR("UART device not ready");
        return;
    }

    // Register async callback (covers TX_DONE, RX_RDY, RX_BUF_REQUEST)
    int err = uart_callback_set(g_dev, uart_callback, nullptr);
    if (err) {
        LOG_ERR("uart_callback_set failed: %d", err);
        return;
    }

    // Kick off continuous DMA RX
    rx_restart();

    // Enqueue startup banner so the host sees the baud rate is correct
    g_uart.log(k_uptime_get_32(), LogLevel::INFO,
               "HCE UART DMA 921600 ready");
    tx_kick();

    LOG_INF("DMA UART ready: TX=PA2 RX=PA3 @ 921600");
}

// ---------------------------------------------------------------------------
// uart_dma_log() — enqueue a structured log packet (ISR-safe via atomic push)
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// uart_dma_rx_pop() — retrieve one received byte (or nullopt if empty)
// ---------------------------------------------------------------------------
std::optional<uint8_t> uart_dma_rx_pop(void) {
    return g_uart.rxPop();
}

// ---------------------------------------------------------------------------
// uart_dma_tx_overflow() — how many log packets were dropped
// ---------------------------------------------------------------------------
uint32_t uart_dma_tx_overflow(void) {
    return g_uart.txOverflow();
}

// ---------------------------------------------------------------------------
// tx_kick() — drain ring to tx scratch buffer and start DMA TX
//             Must only be called when g_tx_busy is acquired (true)
// ---------------------------------------------------------------------------
static void tx_kick(void) {
    do {
        size_t n = g_uart.drainToTxBuffer(g_tx_buf, sizeof(g_tx_buf));
        if (n > 0) {
            int err = uart_tx(g_dev, g_tx_buf, n, SYS_FOREVER_US);
            if (err == 0) {
                return;
            }
            LOG_ERR("uart_tx failed: %d", err);
        }
        g_tx_busy.store(false, std::memory_order_release);
    } while (g_uart.txPending() > 0 && g_tx_busy.exchange(true, std::memory_order_acquire) == false);
}

// ---------------------------------------------------------------------------
// rx_restart() — submit next ping-pong buffer to DMA RX engine
// ---------------------------------------------------------------------------
static void rx_restart(void) {
    uint8_t* buf = g_rx_use_a ? g_rx_buf_a : g_rx_buf_b;
    g_rx_use_a   = !g_rx_use_a;
    int err = uart_rx_enable(g_dev, buf, sizeof(g_rx_buf_a), 1000 /* µs timeout */);
    if (err) LOG_ERR("uart_rx_enable failed: %d", err);
}

// ---------------------------------------------------------------------------
// uart_callback() — Zephyr async UART event handler (called from ISR context)
// ---------------------------------------------------------------------------
static void uart_callback(const struct device* dev,
                          struct uart_event*   evt,
                          void*                user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    switch (evt->type) {

    // TX DMA complete — free the bus and send next batch if any
    case UART_TX_DONE:
        tx_kick();
        break;

    // TX aborted (e.g. timeout) — allow recovery
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

    // RX data ready — push every received byte into the RX ring
    case UART_RX_RDY: {
        const uint8_t* data = evt->data.rx.buf + evt->data.rx.offset;
        size_t         len  = evt->data.rx.len;
        for (size_t i = 0; i < len; ++i) {
            if (!g_uart.rxPush(data[i])) {
                // RX overflow — counted inside RingBuffer
                LOG_WRN_ONCE("RX ring overflow");
            }
        }
        break;
    }

    // Zephyr requests a new RX buffer for double-buffered DMA
    case UART_RX_BUF_REQUEST: {
        uint8_t* buf = g_rx_use_a ? g_rx_buf_a : g_rx_buf_b;
        g_rx_use_a   = !g_rx_use_a;
        uart_rx_buf_rsp(dev, buf, sizeof(g_rx_buf_a));
        break;
    }

    // RX DMA buffer released — nothing to do
    case UART_RX_BUF_RELEASED:
        break;

    // RX disabled (e.g. error) — re-enable
    case UART_RX_DISABLED:
        LOG_WRN("UART RX disabled — restarting");
        rx_restart();
        break;

    // RX stopped externally
    case UART_RX_STOPPED:
        LOG_WRN("UART RX stopped: reason=%d", evt->data.rx_stop.reason);
        break;

    default:
        break;
    }
}

#endif // CONFIG_UART_ASYNC_API