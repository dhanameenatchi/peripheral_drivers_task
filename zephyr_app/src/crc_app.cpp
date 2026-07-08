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

// ── Both CRC strategies ──────────────────────────────────────────────────────
static Crc16Ccitt g_crc16;
static Crc8Maxim  g_crc8;

// ── Codecs for both strategies ───────────────────────────────────────────────
static FrameCodec g_codec16(g_crc16);
static FrameCodec g_codec8(g_crc8);

// ── Demo state ───────────────────────────────────────────────────────────────
static const ICrcStrategy* g_active_crc = &g_crc16;
static uint32_t g_demo_cycle = 0;

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
static size_t  g_rx_expected = 0;  // SOF + Len + Cmd + Payload
static volatile uint32_t g_last_rx_time = 0;

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
static void uart_irq_handler(const struct device* dev, void* /* user_data */) {
    uart_irq_update(dev);
    if (!uart_irq_rx_ready(dev)) {
        return;
    }

    uint8_t c;
    while (uart_fifo_read(dev, &c, 1) == 1) {
        g_last_rx_time = k_uptime_get_32();
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
                g_rx_expected = 3 + c;
                g_rx_state = RxState::WAIT_BODY;
            }
            break;

        case RxState::WAIT_BODY:
            if (g_rx_idx < sizeof(g_rx_buf)) {
                g_rx_buf[g_rx_idx++] = c;
            }
            // Check CRC8-Maxim at (expected + 1) bytes
            if (g_rx_idx == g_rx_expected + 1) {
                auto decoded = g_codec8.decode(g_rx_buf, g_rx_idx);
                if (decoded.has_value()) {
                    uart_send_raw(g_rx_buf, g_rx_idx);
                    g_rx_state = RxState::WAIT_SOF;
                    g_rx_idx = 0;
                    break;
                }
            }
            // Check CRC16-CCITT at (expected + 2) bytes
            if (g_rx_idx == g_rx_expected + 2) {
                auto decoded = g_codec16.decode(g_rx_buf, g_rx_idx);
                if (decoded.has_value()) {
                    uart_send_raw(g_rx_buf, g_rx_idx);
                } else {
                    if (!g_loopback_active) {
                        LOG_WRN("CRC ERROR - frame dropped");
                    }
                }
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
    LOG_INF("Active       : %s", g_active_crc->name());
    LOG_INF("====================================================");

    // ── Self-test: encode → decode round-trip ────────────────────────────────
    Frame f;
    f.cmd = 0x01;
    f.payload_len = 5;
    __builtin_memcpy(f.payload, "ALARM", 5);

    uint8_t buf[FrameCodec::MAX_FRAME_BYTES];
    size_t n = g_codec16.encode(f, buf, sizeof(buf));
    auto decoded = g_codec16.decode(buf, n);

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
// Alternates between CRC16-CCITT and CRC8-Maxim each cycle
// =============================================================================
void crc_app_demo() {
    uint32_t ts = k_uptime_get_32();

    // ── Swap CRC strategy every cycle ────────────────────────────────────────
    if (g_demo_cycle % 2 == 0) {
        g_active_crc = &g_crc16;
    } else {
        g_active_crc = &g_crc8;
    }
    // Reconstruct codec with active strategy (placement-new on static storage)
    static uint8_t codec_mem[sizeof(FrameCodec)];
    FrameCodec* codec = new (codec_mem) FrameCodec(*g_active_crc);

    LOG_INF("====================================================");
    LOG_INF("[Cycle %u] Timestamp: %u ms", g_demo_cycle, ts);
    LOG_INF("Active CRC Strategy: %s", g_active_crc->name());
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
    size_t n = codec->encode(f, buf, sizeof(buf));

    if (n == 0) {
        LOG_ERR("  Encode FAILED");
        ++g_demo_cycle;
        return;
    }

    print_hex("  Encoded frame", buf, n);

    // ── Step 3: Decode the valid frame ───────────────────────────────────────
    LOG_INF("[DECODE] Valid frame:");
    auto decoded = codec->decode(buf, n);

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

    auto bad_decode = codec->decode(corrupted, n);
    if (!bad_decode.has_value()) {
        LOG_INF("  CRC verify    : FAIL (error correctly detected)");
    } else {
        LOG_ERR("  CRC verify    : PASS (ERROR NOT DETECTED — bug!)");
    }

    LOG_INF("====================================================");

    ++g_demo_cycle;
}

// =============================================================================
// crc_loopback_service — called periodically in Loopback Mode
// Performs only binary UART loopback
// =============================================================================
void crc_loopback_service() {
    static bool initialized = false;
    if (!initialized) {
        g_loopback_active = true;
        g_uart_dev = DEVICE_DT_GET(DT_NODELABEL(usart2));
        if (device_is_ready(g_uart_dev)) {
            uart_irq_callback_set(g_uart_dev, uart_irq_handler);
            uart_irq_rx_enable(g_uart_dev);
        }
        initialized = true;
    }

    // Timeout reset: if rx has been idle for > 50 ms in the middle of a frame, reset.
    if (g_rx_state != RxState::WAIT_SOF && (k_uptime_get_32() - g_last_rx_time) > 50) {
        g_rx_state = RxState::WAIT_SOF;
        g_rx_idx = 0;
    }

    // No sleep here: handled cooperatively by main scheduler loop.
}

FrameCodec& crc_get_codec() {
    return g_codec16;
}
