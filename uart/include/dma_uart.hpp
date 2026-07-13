#pragma once
// =============================================================================
// Log packet + DMA UART driver
// Structured packet: { timestamp_ms, level, message[32] }
// DMA transfers packets without blocking the CPU.
// =============================================================================
#include "ring_buffer.hpp"
#include <cstdint>
#include <optional>

// ---------------------------------------------------------------------------
// Log packet — fixed-size, no heap
// ---------------------------------------------------------------------------
enum class LogLevel : uint8_t { DEBUG = 0, INFO, WARN, ERROR };

struct LogPacket {
    uint32_t timestamp_ms;
    LogLevel level;
    char     message[64];

    static LogPacket make(uint32_t ts, LogLevel lvl, const char* msg);
};

// ---------------------------------------------------------------------------
// DmaUart — background TX via DMA, RX into ring buffer
// On host: DMA calls are replaced by direct memcpy for testing
// ---------------------------------------------------------------------------
class DmaUart {
public:
    static constexpr size_t TX_BUF_SIZE = 256;  // power-of-two
    static constexpr size_t RX_BUF_SIZE = 256;
    static constexpr uint32_t BAUD = 921600;    

    DmaUart() = default;

    // Queue a log packet for DMA transmission. Returns false on overflow.
    bool log(uint32_t ts, LogLevel lvl, const char* msg);

    // In real Zephyr this is called from DMA complete ISR;
    // on host we call it explicitly to drain the ring buffer.
    size_t drainToTxBuffer(uint8_t* out, size_t max_bytes);

    // Push raw byte to RX ring (called from DMA RX ISR on hardware)
    bool rxPush(uint8_t byte);
    std::optional<uint8_t> rxPop();

    [[nodiscard]] uint32_t txOverflow() const;
    [[nodiscard]] uint32_t rxOverflow() const;
    [[nodiscard]] size_t   txPending()  const;

private:
    RingBuffer<LogPacket, TX_BUF_SIZE> tx_ring_;
    RingBuffer<uint8_t,  RX_BUF_SIZE> rx_ring_;
};
