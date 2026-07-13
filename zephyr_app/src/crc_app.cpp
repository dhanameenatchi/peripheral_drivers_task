// =============================================================================
// crc_app.cpp — CRC Strategy + FrameCodec hardware demonstration
// Includes: Demo Mode and Loopback Mode services
// Medical context: IEC 60601-1-8 alarm communication frame integrity
// =============================================================================
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <cstring>
#include <cstdio>
#include "frame_codec.hpp"

LOG_MODULE_REGISTER(crc_app, LOG_LEVEL_INF);
static volatile uint32_t g_irq_count = 0;
// ── Both CRC strategies ──────────────────────────────────────────────────────
static Crc16Ccitt g_crc16;
static Crc8Maxim  g_crc8;

// ── Codecs for both strategies ───────────────────────────────────────────────
static FrameCodec g_codec16(g_crc16);
static FrameCodec g_codec8(g_crc8);

// ── Build-time selected active strategy & codec ──────────────────────────────
#ifdef CONFIG_CRC_STRATEGY_CRC8
static const ICrcStrategy* const g_active_strategy = &g_crc8;
static const FrameCodec* const g_active_codec = &g_codec8;
#else
static const ICrcStrategy* const g_active_strategy = &g_crc16;
static const FrameCodec* const g_active_codec = &g_codec16;
#endif

// ── Demo state ───────────────────────────────────────────────────────────────
#ifndef CONFIG_CRC_LOOPBACK_MODE
static uint32_t g_demo_cycle = 0;
#endif

// ── UART loopback state ──────────────────────────────────────────────────────
static const struct device* g_uart_dev = nullptr;
static bool g_loopback_active = false;

// Frame receive buffer (max frame = 3 + 32 + 2 = 37 bytes)
static uint8_t  g_rx_buf[FrameCodec::MAX_FRAME_BYTES];
static volatile size_t g_rx_idx = 0;

// Frame assembly state machine
enum class RxState : uint8_t {
    WAIT_SOF,       // waiting for 0xAA
    WAIT_LEN,       // waiting for payload length byte
    WAIT_BODY,      // accumulating cmd + payload + CRC
};
static volatile RxState g_rx_state = RxState::WAIT_SOF;
static volatile size_t  g_rx_expected_total = 0;  // total frame bytes including CRC
static volatile uint32_t g_last_rx_time = 0;

// ISR→main-loop handoff: ISR sets this flag when a complete frame is buffered
static volatile bool   g_rx_frame_ready = false;
static volatile size_t g_rx_frame_len   = 0;

// ---------------------------------------------------------------------------
// Helper: send raw binary bytes over UART (blocking, for loopback response)
// ---------------------------------------------------------------------------
static void uart_send_raw(const uint8_t* data, size_t len) {
    if (!g_uart_dev) return;
    for (size_t i = 0; i < len; ++i) {
        uart_poll_out(g_uart_dev, data[i]);
    }
}

// ---------------------------------------------------------------------------
// UART IRQ handler — state-machine byte accumulator
// ---------------------------------------------------------------------------
static const char hex_lut[] = "0123456789ABCDEF";

static void uart_irq_handler(const struct device* dev, void* /* user_data */) {
    // uart_poll_out(dev, '.');
    uart_irq_update(dev);
    (void)uart_err_check(dev);

    if (!uart_irq_rx_ready(dev)) {
        return;
    }
    uint8_t c;
    while (uart_fifo_read(dev, &c, 1) == 1) {
        g_irq_count++;
        // DIAG: print each received byte as 2-char hex
        // uart_poll_out(dev, hex_lut[(c >> 4) & 0x0F]);
        // uart_poll_out(dev, hex_lut[c & 0x0F]);

        g_last_rx_time = k_uptime_get_32();

        // If a frame is pending main-loop processing, discard incoming bytes
        if (g_rx_frame_ready) {
            continue;
        }

        switch (g_rx_state) {
        case RxState::WAIT_SOF:
            if (c == Frame::SOF) {
                g_rx_buf[0] = c;
                g_rx_idx = 1;
                g_rx_state = RxState::WAIT_LEN;
            }
            break;

        case RxState::WAIT_LEN:
            g_rx_buf[1] = c;
            g_rx_idx = 2;
            if (c > Frame::MAX_PAYLOAD) {
                g_rx_state = RxState::WAIT_SOF;
                g_rx_idx = 0;
            } else {
                // Pre-compute total expected bytes (header + payload + CRC)
                // using the build-time CRC size constant to avoid virtual
                // calls from ISR context.
#ifdef CONFIG_CRC_STRATEGY_CRC8
                g_rx_expected_total = 3 + c + 1;  // CRC8 = 1 byte
#else
                g_rx_expected_total = 3 + c + 2;  // CRC16 = 2 bytes
#endif
                g_rx_state = RxState::WAIT_BODY;
            }
            break;

        case RxState::WAIT_BODY:
            if (g_rx_idx < sizeof(g_rx_buf)) {
                g_rx_buf[g_rx_idx++] = c;
            }
            // Signal main loop when all expected bytes have arrived
            if (g_rx_idx >= g_rx_expected_total) {
                g_rx_frame_len   = g_rx_idx;
                g_rx_frame_ready = true;  // handoff to main loop
                g_rx_state = RxState::WAIT_SOF;
                g_rx_idx = 0;
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: print a byte buffer in hex
// ---------------------------------------------------------------------------
static void print_hex(const char* label, const uint8_t* buf, size_t len) {
    char hex[128];
    size_t pos = 0;
    for (size_t i = 0; i < len && pos + 3 < sizeof(hex); ++i) {
        pos += static_cast<size_t>(
            snprintf(hex + pos, sizeof(hex) - pos, "%02X ", buf[i]));
    }
    if (pos > 0) hex[pos - 1] = '\0';  // trim trailing space
    LOG_INF("%s [%u B]: %s", label, (unsigned)len, hex);
}

// ---------------------------------------------------------------------------
// Helper: print decoded payload as ASCII (printable bytes only)
// ---------------------------------------------------------------------------
static void print_payload_ascii(const uint8_t* payload, uint8_t len) {
    char ascii[Frame::MAX_PAYLOAD + 1];
    for (uint8_t i = 0; i < len; ++i) {
        ascii[i] = (payload[i] >= 0x20 && payload[i] <= 0x7E)
                   ? static_cast<char>(payload[i]) : '.';
    }
    ascii[len] = '\0';
    LOG_INF("  Payload ASCII : %s", ascii);
}

// =============================================================================
// crc_app_init — self-test, print configuration
// =============================================================================
void crc_app_init() {
    LOG_INF("====================================================");
    LOG_INF("CRC FrameCodec Module Initialized");
    LOG_INF("Frame format : [SOF 0xAA | Len | Cmd | Payload <=32B | CRC]");
    LOG_INF("Strategy 1   : CRC16-CCITT (poly=0x1021, init=0xFFFF)");
    LOG_INF("Strategy 2   : CRC8-Maxim  (poly=0x31,   init=0x00)");
    LOG_INF("Active       : %s", g_active_strategy->name());
    LOG_INF("====================================================");

    // ── Self-test: encode → decode round-trip ────────────────────────────────
    Frame f;
    f.cmd = 0x01;
    f.payload_len = 5;
    __builtin_memcpy(f.payload, "ALARM", 5);

    uint8_t buf[FrameCodec::MAX_FRAME_BYTES];
    size_t n = g_active_codec->encode(f, buf, sizeof(buf));
    auto decoded = g_active_codec->decode(buf, n);

    if (decoded.has_value()) {
        LOG_INF("CRC self-test PASSED (cmd=0x%02X, payload=%u bytes)",
                decoded->cmd, decoded->payload_len);
    } else {
        LOG_ERR("CRC self-test FAILED — check frame_codec.hpp");
    }
    LOG_INF("====================================================");
}

// =============================================================================
// crc_app_demo — called periodically in Demo Mode
// Guarded by CRC_LOOPBACK_MODE config to prevent console log corruption
// =============================================================================
void crc_app_demo() {
#ifndef CONFIG_CRC_LOOPBACK_MODE
    uint32_t ts = k_uptime_get_32();

    LOG_INF("====================================================");
    LOG_INF("[Cycle %u] Timestamp: %u ms", g_demo_cycle, ts);
    LOG_INF("Active CRC Strategy: %s", g_active_strategy->name());
    LOG_INF("----------------------------------------------------");

    // ── Step 1: Build a sample frame ─────────────────────────────────────────
    Frame f;
    f.cmd = static_cast<uint8_t>(0x10 + (g_demo_cycle % 4));
    const char* payloads[] = {"ALARM", "TEMP_HI", "PRESS_LO", "SPO2_OK"};
    const char* msg = payloads[g_demo_cycle % 4];
    f.payload_len = static_cast<uint8_t>(strlen(msg));
    __builtin_memcpy(f.payload, msg, f.payload_len);

    LOG_INF("[ENCODE] cmd=0x%02X payload=\"%s\" (%u B)",
            f.cmd, msg, f.payload_len);

    // ── Step 2: Encode ───────────────────────────────────────────────────────
    uint8_t buf[FrameCodec::MAX_FRAME_BYTES];
    size_t n = g_active_codec->encode(f, buf, sizeof(buf));

    if (n == 0) {
        LOG_ERR("  Encode FAILED");
        ++g_demo_cycle;
        return;
    }

    print_hex("  Encoded frame", buf, n);

    // ── Step 3: Decode the valid frame ───────────────────────────────────────
    LOG_INF("[DECODE] Valid frame:");
    auto decoded = g_active_codec->decode(buf, n);

    if (decoded.has_value()) {
        LOG_INF("  CRC verify    : PASS");
        LOG_INF("  Command       : 0x%02X", decoded->cmd);
        LOG_INF("  Payload len   : %u", decoded->payload_len);
        print_payload_ascii(decoded->payload, decoded->payload_len);
    } else {
        LOG_ERR("  CRC verify    : FAIL (unexpected!)");
    }

    // ── Step 4: Single-bit error injection ───────────────────────────────────
    LOG_INF("----------------------------------------------------");
    LOG_INF("[ERROR INJECTION] Flipping bit 0 of payload byte 0");

    uint8_t corrupted[FrameCodec::MAX_FRAME_BYTES];
    __builtin_memcpy(corrupted, buf, n);
    corrupted[3] ^= 0x01;  // flip bit 0 of first payload byte

    print_hex("  Corrupted frame", corrupted, n);

    auto bad_decode = g_active_codec->decode(corrupted, n);
    if (!bad_decode.has_value()) {
        LOG_INF("  CRC verify    : FAIL (error correctly detected)");
    } else {
        LOG_ERR("  CRC verify    : PASS (ERROR NOT DETECTED — bug!)");
    }

    LOG_INF("====================================================");

    ++g_demo_cycle;
#endif
}

// =============================================================================
// crc_loopback_service — called periodically from main loop
// ISR accumulates bytes into g_rx_buf and sets g_rx_frame_ready.
// This function performs decode + echo in thread context (safe for
// virtual dispatch and uart_poll_out).
// =============================================================================
void crc_loopback_service() {
    static bool initialized = false;

    if (!initialized) {
        g_loopback_active = true;
        g_uart_dev = DEVICE_DT_GET(DT_NODELABEL(usart2));

        bool ready = device_is_ready(g_uart_dev);

        // Direct uart_poll_out diagnostic (bypasses disabled log backend)
        // {
        //     const char* msg = ready ? "LOOPBACK_READY\r\n"
        //                             : "LOOPBACK_NOT_READY\r\n";
        //     for (const char* p = msg; *p; ++p) {
        //         uart_poll_out(g_uart_dev, *p);
        //     }
        // }

        if (ready) {
    // 1. Disable RX interrupts during configuration setup
    uart_irq_rx_disable(g_uart_dev);

    // 2. Clear any stale bytes lingering in the hardware FIFO
    uint8_t dummy;
    while (uart_fifo_read(g_uart_dev, &dummy, 1) > 0) {
        // Purging buffer
    }

    // 3. Set the callback using standard interrupt-driven API
    uart_irq_callback_set(g_uart_dev, uart_irq_handler);
    // Direct output verification message
    // {
    //     char buf[24];
    //     int n = snprintf(buf, sizeof(buf), "CB_RC=%d\r\n", cb_rc);
    //     for (int i = 0; i < n; ++i) {
    //         uart_poll_out(g_uart_dev, buf[i]);
    //     }
    // }

    // 4. CRITICAL: Trigger the internal interrupt routing
    uart_irq_rx_enable(g_uart_dev);
}
        initialized = true;
    }

    // ── Process completed frame handed off by ISR ────────────────────────────
    if (g_rx_frame_ready) {
        size_t len = g_rx_frame_len;

        // Verify the CRC of the received frame before echoing
        if (g_active_codec->decode(g_rx_buf, len).has_value()) {
            uart_send_raw(g_rx_buf, len);
        }

        // Release buffer for next frame atomically
        unsigned int key = irq_lock();
        g_rx_frame_ready = false;
        g_rx_frame_len = 0;
        g_rx_expected_total = 0;
        g_rx_idx = 0;
        g_rx_state = RxState::WAIT_SOF;
        irq_unlock(key);
    }

    // Timeout reset: if rx has been idle for > 200 ms in the middle of a frame, reset.
    if (g_rx_state != RxState::WAIT_SOF && (k_uptime_get_32() - g_last_rx_time) > 200) {
        unsigned int key = irq_lock();
        // Double check after lock
        if (g_rx_state != RxState::WAIT_SOF && (k_uptime_get_32() - g_last_rx_time) > 200) {
            g_rx_state = RxState::WAIT_SOF;
            g_rx_idx = 0;
        }
        irq_unlock(key);
    }
}

FrameCodec& crc_get_codec() {
    return const_cast<FrameCodec&>(*g_active_codec);
}

bool crc_is_busy(void) {
    return g_rx_state != RxState::WAIT_SOF;
}
