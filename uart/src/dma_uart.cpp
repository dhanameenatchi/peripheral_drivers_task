#include "dma_uart.hpp"
#include <cstring>
#include <cstdio>
#include <type_traits>
#include <algorithm>

// ---------------------------------------------------------------------------
// Static Assertions
// ---------------------------------------------------------------------------
static_assert(std::is_trivially_copyable_v<LogPacket>, "LogPacket must be trivially copyable");
static_assert(sizeof(LogPacket) == 40, "LogPacket layout must be exactly 40 bytes");

static_assert(static_cast<uint8_t>(LogLevel::DEBUG) == 0, "LogLevel::DEBUG enum order mismatch");
static_assert(static_cast<uint8_t>(LogLevel::INFO)  == 1, "LogLevel::INFO enum order mismatch");
static_assert(static_cast<uint8_t>(LogLevel::WARN)  == 2, "LogLevel::WARN enum order mismatch");
static_assert(static_cast<uint8_t>(LogLevel::ERROR) == 3, "LogLevel::ERROR enum order mismatch");

// ---------------------------------------------------------------------------
// LogPacket Implementation
// ---------------------------------------------------------------------------
LogPacket LogPacket::make(uint32_t ts, LogLevel lvl, const char* msg) {
    LogPacket p{};
    p.timestamp_ms = ts;
    p.level = lvl;
    if (msg != nullptr) {
        std::strncpy(p.message, msg, sizeof(p.message) - 1);
        p.message[sizeof(p.message) - 1] = '\0';
    } else {
        p.message[0] = '\0';
    }
    return p;
}

// ---------------------------------------------------------------------------
// DmaUart Implementation
// ---------------------------------------------------------------------------
bool DmaUart::log(uint32_t ts, LogLevel lvl, const char* msg) {
    return tx_ring_.push(LogPacket::make(ts, lvl, msg));
}

size_t DmaUart::drainToTxBuffer(uint8_t* out, size_t max_bytes) {
    size_t written = 0;
    while (written < max_bytes) {
        const LogPacket* pkt_ptr = tx_ring_.peek();
        if (!pkt_ptr) break;

        static constexpr const char* LEVEL_STR[] = {"DBG", "INF", "WRN", "ERR"};
        const char* level_name = "UNK";
        uint8_t lvl_idx = static_cast<uint8_t>(pkt_ptr->level);
        if (lvl_idx < 4) {
            level_name = LEVEL_STR[lvl_idx];
        }

        int n = std::snprintf(
            nullptr,
            0,
            "T=%u [%s] %s\r\n",
            pkt_ptr->timestamp_ms,
            level_name,
            pkt_ptr->message
        );
        if (n <= 0) {
            // Discard invalid packet to avoid infinite loop
            tx_ring_.pop();
            continue;
        }

        size_t required = static_cast<size_t>(n);
        if (written + required > max_bytes) {
            // Does not fit, retain packet
            break;
        }

        int written_bytes = std::snprintf(
            reinterpret_cast<char*>(out + written),
            max_bytes - written,
            "T=%u [%s] %s\r\n",
            pkt_ptr->timestamp_ms,
            level_name,
            pkt_ptr->message
        );
        if (written_bytes <= 0) {
            tx_ring_.pop();
            continue;
        }
        written += static_cast<size_t>(written_bytes);
        tx_ring_.pop();
    }
    return written;
}

bool DmaUart::rxPush(uint8_t byte) {
    return rx_ring_.push(byte);
}

std::optional<uint8_t> DmaUart::rxPop() {
    return rx_ring_.pop();
}

uint32_t DmaUart::txOverflow() const {
    return tx_ring_.overflowCount();
}

uint32_t DmaUart::rxOverflow() const {
    return rx_ring_.overflowCount();
}

size_t DmaUart::txPending() const {
    return tx_ring_.size();
}
