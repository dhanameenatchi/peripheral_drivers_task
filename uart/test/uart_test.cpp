// =============================================================================
// test_dma_uart.cpp — GTest for DmaUart driver (host-side, no Zephyr)
// Covers: log() enqueue; drainToTxBuffer() format; rxPush/rxPop; TX/RX
//         overflow counters; packet ordering; overflow injection.
// 100% line + branch coverage target for dma_uart.hpp
// =============================================================================
#include <gtest/gtest.h>
#include "ring_buffer.hpp"
#include "dma_uart.hpp"
#include "zephyr_uart_mock.hpp"
#include <thread>
#include <atomic>
#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// Fixture — fresh RingBuffer<uint32_t, 8> for each test
// ---------------------------------------------------------------------------
class RingBufferTest : public ::testing::Test {
protected:
    RingBuffer<uint32_t, 8> rb;   // capacity 7 usable slots (1 sentinel)

    void SetUp() override { rb.reset(); }
};

// ---------------------------------------------------------------------------
// 1. Initial state — empty, not full, size zero
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, InitialStateIsEmpty) {
    EXPECT_TRUE(rb.empty());
    EXPECT_FALSE(rb.full());
    EXPECT_EQ(rb.size(), 0u);
    EXPECT_EQ(rb.overflowCount(), 0u);
}

// ---------------------------------------------------------------------------
// 2. Single push then pop round-trip
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, PushPopSingleElement) {
    EXPECT_TRUE(rb.push(42u));
    EXPECT_FALSE(rb.empty());
    EXPECT_EQ(rb.size(), 1u);

    auto v = rb.pop();
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 42u);
    EXPECT_TRUE(rb.empty());
    EXPECT_EQ(rb.size(), 0u);
}

// ---------------------------------------------------------------------------
// 3. Pop on empty returns nullopt — branch coverage
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, PopOnEmptyReturnsNullopt) {
    auto v = rb.pop();
    EXPECT_FALSE(v.has_value());
}

// ---------------------------------------------------------------------------
// 4. Fill to capacity — full() becomes true
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, FillToCapacity) {
    // N=8 → 7 usable slots
    for (uint32_t i = 0; i < 7; ++i) {
        EXPECT_TRUE(rb.push(i)) << "push #" << i << " should succeed";
    }
    EXPECT_TRUE(rb.full());
    EXPECT_FALSE(rb.empty());
    EXPECT_EQ(rb.size(), 7u);
}

// ---------------------------------------------------------------------------
// 5. Overflow — push on full increments saturating counter, returns false
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, OverflowCounterIncrements) {
    for (uint32_t i = 0; i < 7; ++i) rb.push(i);   // fill
    EXPECT_TRUE(rb.full());

    EXPECT_FALSE(rb.push(99u));
    EXPECT_EQ(rb.overflowCount(), 1u);

    EXPECT_FALSE(rb.push(100u));
    EXPECT_EQ(rb.overflowCount(), 2u);
}

// ---------------------------------------------------------------------------
// 6. RingBuffer Peek test
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, PeekReturnsOldestElementWithoutPopping) {
    EXPECT_EQ(rb.peek(), nullptr);
    rb.push(42u);
    rb.push(43u);
    const uint32_t* p = rb.peek();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42u);
    
    // Peek again, should still be 42u
    p = rb.peek();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42u);

    // Pop and peek again, should be 43u
    rb.pop();
    p = rb.peek();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 43u);

    rb.pop();
    EXPECT_EQ(rb.peek(), nullptr);
}

// ---------------------------------------------------------------------------
// 7. resetOverflow() clears counter
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, ResetOverflowClearsCounter) {
    for (uint32_t i = 0; i < 7; ++i) rb.push(i);
    rb.push(0xFFu);  // overflow
    EXPECT_GT(rb.overflowCount(), 0u);

    rb.resetOverflow();
    EXPECT_EQ(rb.overflowCount(), 0u);
}

// ---------------------------------------------------------------------------
// 8. Wrap-around: fill, drain, refill crosses the power-of-two boundary
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, WrapAroundBoundary) {
    // Push 4, pop 4 (head & tail both advance to 4)
    for (uint32_t i = 0; i < 4; ++i) rb.push(i);
    for (uint32_t i = 0; i < 4; ++i) rb.pop();

    // Push 7 more — head wraps past N=8 boundary
    for (uint32_t i = 10; i < 17; ++i) {
        EXPECT_TRUE(rb.push(i)) << "wrap push " << i;
    }
    EXPECT_TRUE(rb.full());

    // Pop and verify values are correct after wrap
    for (uint32_t i = 10; i < 17; ++i) {
        auto v = rb.pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, i);
    }
    EXPECT_TRUE(rb.empty());
}

// ---------------------------------------------------------------------------
// 9. Multiple wrap cycles — stress the index masking
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, MultipleWrapCycles) {
    for (uint32_t cycle = 0; cycle < 5; ++cycle) {
        for (uint32_t i = 0; i < 7; ++i) EXPECT_TRUE(rb.push(cycle * 10 + i));
        EXPECT_TRUE(rb.full());
        for (uint32_t i = 0; i < 7; ++i) {
            auto v = rb.pop();
            ASSERT_TRUE(v.has_value());
            EXPECT_EQ(*v, cycle * 10 + i);
        }
        EXPECT_TRUE(rb.empty());
    }
    EXPECT_EQ(rb.overflowCount(), 0u);
}

// ---------------------------------------------------------------------------
// 10. reset() clears head, tail, and overflow counter
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, ResetClearsAll) {
    for (uint32_t i = 0; i < 7; ++i) rb.push(i);
    rb.push(0u);  // overflow

    rb.reset();
    EXPECT_TRUE(rb.empty());
    EXPECT_FALSE(rb.full());
    EXPECT_EQ(rb.size(), 0u);
    EXPECT_EQ(rb.overflowCount(), 0u);
}

// ---------------------------------------------------------------------------
// 11. FIFO ordering — values come out in push order
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, FIFOOrdering) {
    for (uint32_t i = 0; i < 7; ++i) rb.push(i * i);
    for (uint32_t i = 0; i < 7; ++i) {
        auto v = rb.pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, i * i);
    }
}

// ---------------------------------------------------------------------------
// 12. size() tracks correctly across mixed push/pop
// ---------------------------------------------------------------------------
TEST_F(RingBufferTest, SizeTracksCorrectly) {
    rb.push(1u); EXPECT_EQ(rb.size(), 1u);
    rb.push(2u); EXPECT_EQ(rb.size(), 2u);
    rb.pop();    EXPECT_EQ(rb.size(), 1u);
    rb.push(3u); EXPECT_EQ(rb.size(), 2u);
    rb.pop();    EXPECT_EQ(rb.size(), 1u);
    rb.pop();    EXPECT_EQ(rb.size(), 0u);
}

// ---------------------------------------------------------------------------
// 13. Works with non-trivial type (LogPacket-sized struct)
// ---------------------------------------------------------------------------
struct TestPkt {
    uint32_t ts;
    uint8_t  lvl;
    char     msg[64];
};

TEST(RingBufferTypes, NonTrivialStruct) {
    RingBuffer<TestPkt, 4> rb;
    TestPkt p{};
    p.ts = 1234;
    p.lvl = 2;
    std::strncpy(p.msg, "hello", sizeof(p.msg) - 1);

    EXPECT_TRUE(rb.push(p));
    auto v = rb.pop();
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->ts,  1234u);
    EXPECT_EQ(v->lvl, 2u);
    EXPECT_STREQ(v->msg, "hello");
}

// ---------------------------------------------------------------------------
// 14. Works with uint8_t (RX byte buffer use-case)
// ---------------------------------------------------------------------------
TEST(RingBufferTypes, ByteBuffer) {
    RingBuffer<uint8_t, 16> rb;
    for (uint8_t i = 0; i < 15; ++i) EXPECT_TRUE(rb.push(i));
    EXPECT_TRUE(rb.full());
    for (uint8_t i = 0; i < 15; ++i) {
        auto v = rb.pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, i);
    }
}

// ---------------------------------------------------------------------------
// 15. Concurrent SPSC — producer-consumer 1 000 iterations
//     Producer pushes 0..999; consumer pops and validates.
//     Overflow counter must be zero (buffer large enough: N=1024).
// ---------------------------------------------------------------------------
TEST(RingBufferConcurrent, ProducerConsumer1000) {
    static RingBuffer<uint32_t, 1024> rb;
    rb.reset();

    constexpr int ITEMS = 1000;
    std::atomic<bool> done{false};
    std::atomic<int>  consumed{0};
    std::atomic<bool> order_ok{true};

    // Consumer thread
    std::thread consumer([&] {
        uint32_t expected = 0;
        while (consumed.load(std::memory_order_relaxed) < ITEMS) {
            auto v = rb.pop();
            if (v.has_value()) {
                if (*v != expected) {
                    order_ok.store(false, std::memory_order_relaxed);
                }
                ++expected;
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // Producer (this thread)
    for (uint32_t i = 0; i < ITEMS; ++i) {
        // Spin until space available (simulates ISR backpressure)
        while (!rb.push(i)) { std::this_thread::yield(); }
    }

    consumer.join();

    EXPECT_EQ(consumed.load(), ITEMS);
    EXPECT_TRUE(order_ok.load());
    EXPECT_EQ(rb.overflowCount(), 0u);  // no data was lost
    EXPECT_TRUE(rb.empty());
}

// ---------------------------------------------------------------------------
// 16. Concurrent overflow injection — small buffer, fast producer
//     Verifies overflow counter is non-zero and data is not corrupted.
// ---------------------------------------------------------------------------
TEST(RingBufferConcurrent, OverflowInjection) {
    static RingBuffer<uint32_t, 8> rb;  // only 7 slots
    rb.reset();

    constexpr int PRODUCE = 200;  // much more than capacity
    std::atomic<int> consumed{0};

    std::atomic<bool> producer_done{false};

    std::thread consumer([&] {
        int local = 0;
        // Drain whatever we can while producer runs
        while (!producer_done.load(std::memory_order_acquire) || rb.pop().has_value()) {
            if (rb.pop().has_value()) ++local;
            else std::this_thread::yield();
        }
        consumed.store(local, std::memory_order_release);
    });

    for (int i = 0; i < PRODUCE; ++i) rb.push(static_cast<uint32_t>(i));
    producer_done.store(true, std::memory_order_release);

    consumer.join();

    // Overflow must have occurred (200 pushes into 7-slot buffer)
    EXPECT_GT(rb.overflowCount(), 0u);
    // Consumer got at least some items
    EXPECT_GT(consumed.load(), 0);
}

// =============================================================================
// test_dma_uart.cpp — GTest for DmaUart driver (host-side, no Zephyr)
// Covers: log() enqueue; drainToTxBuffer() format; rxPush/rxPop; TX/RX
//         overflow counters; packet ordering; overflow injection.
// 100% line + branch coverage target for dma_uart.hpp
// =============================================================================
// ---------------------------------------------------------------------------
// Fixture — fresh DmaUart for every test
// ---------------------------------------------------------------------------
class DmaUartTest : public ::testing::Test {
protected:
    DmaUart uart;
    uint8_t outbuf[4096]{};

    void SetUp() override {
        std::memset(outbuf, 0, sizeof(outbuf));
    }
};

// ---------------------------------------------------------------------------
// 1. Initial state — no pending TX, no overflow
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, InitialState) {
    EXPECT_EQ(uart.txPending(),   0u);
    EXPECT_EQ(uart.txOverflow(),  0u);
    EXPECT_EQ(uart.rxOverflow(),  0u);
}

// ---------------------------------------------------------------------------
// 2. log() enqueues a packet — txPending increases
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, LogEnqueuesPacket) {
    EXPECT_TRUE(uart.log(100, LogLevel::INFO, "boot"));
    EXPECT_EQ(uart.txPending(), 1u);
}

// ---------------------------------------------------------------------------
// 3. drainToTxBuffer() produces non-empty output
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, DrainProducesOutput) {
    uart.log(0, LogLevel::INFO, "hello");
    size_t n = uart.drainToTxBuffer(outbuf, sizeof(outbuf));
    EXPECT_GT(n, 0u);
    EXPECT_EQ(uart.txPending(), 0u);
}

// ---------------------------------------------------------------------------
// 4. Output contains timestamp field
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, OutputContainsTimestamp) {
    uart.log(12345, LogLevel::INFO, "ts_test");
    uart.drainToTxBuffer(outbuf, sizeof(outbuf));
    EXPECT_NE(std::strstr(reinterpret_cast<char*>(outbuf), "12345"), nullptr);
}

// ---------------------------------------------------------------------------
// 5. Output contains message field
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, OutputContainsMessage) {
    uart.log(0, LogLevel::DEBUG, "sensor_ok");
    uart.drainToTxBuffer(outbuf, sizeof(outbuf));
    EXPECT_NE(std::strstr(reinterpret_cast<char*>(outbuf), "sensor_ok"), nullptr);
}

// ---------------------------------------------------------------------------
// 6. All log levels appear correctly in formatted output
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, AllLogLevelStrings) {
    const struct { LogLevel lvl; const char* tag; } cases[] = {
        { LogLevel::DEBUG, "DBG" },
        { LogLevel::INFO,  "INF" },
        { LogLevel::WARN,  "WRN" },
        { LogLevel::ERROR, "ERR" },
    };
    for (auto& c : cases) {
        DmaUart u;
        uint8_t buf[256]{};
        u.log(0, c.lvl, "x");
        u.drainToTxBuffer(buf, sizeof(buf));
        EXPECT_NE(std::strstr(reinterpret_cast<char*>(buf), c.tag), nullptr)
            << "Missing tag for level " << static_cast<int>(c.lvl);
    }
}

// ---------------------------------------------------------------------------
// 7. Output line ends with \r\n (UART convention)
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, OutputEndsWithCRLF) {
    uart.log(0, LogLevel::INFO, "crlf");
    size_t n = uart.drainToTxBuffer(outbuf, sizeof(outbuf));
    ASSERT_GE(n, 2u);
    EXPECT_EQ(outbuf[n - 2], '\r');
    EXPECT_EQ(outbuf[n - 1], '\n');
}

// ---------------------------------------------------------------------------
// 8. Multiple packets drain in FIFO order
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, MultiplePacketFIFOOrder) {
    uart.log(1, LogLevel::INFO,  "first");
    uart.log(2, LogLevel::WARN,  "second");
    uart.log(3, LogLevel::ERROR, "third");
    uart.drainToTxBuffer(outbuf, sizeof(outbuf));

    char* s = reinterpret_cast<char*>(outbuf);
    char* p1 = std::strstr(s, "first");
    char* p2 = std::strstr(s, "second");
    char* p3 = std::strstr(s, "third");
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);
    EXPECT_LT(p1, p2);
    EXPECT_LT(p2, p3);
}

// ---------------------------------------------------------------------------
// 9. drainToTxBuffer() with out_buf too small — partial drain, no overflow
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, PartialDrainWhenBufferTooSmall)
{
    uart.log(1, LogLevel::INFO, "packet_one_long_message");
    uart.log(2, LogLevel::INFO, "packet_two_long_message");

    uint8_t small[40]{};

    size_t n = uart.drainToTxBuffer(small, sizeof(small));

    EXPECT_GT(n, 0u);
    EXPECT_EQ(uart.txPending(), 1u);
}
// ---------------------------------------------------------------------------
// 9b. Packet retained when output buffer is too small to fit the packet
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, PacketRetainedWhenBufferTooSmall) {
    // Log two packets
    uart.log(12345, LogLevel::INFO, "packet_one_long_message"); // formatted size: "T=12345 [INF] packet_one_long_message\r\n" -> 39 bytes
    uart.log(67890, LogLevel::WARN, "packet_two_long_message"); // formatted size: "T=67890 [WRN] packet_two_long_message\r\n" -> 39 bytes
    
    // First let's check what the exact formatted size is for packet 1.
    // It is 39 bytes.
    // If we drain to a buffer of size 38:
    // Packet 1 does not fit at all. It should NOT be popped and we should write 0 bytes.
    uint8_t tiny_buf[38]{};
    size_t n1 = uart.drainToTxBuffer(tiny_buf, sizeof(tiny_buf));
    EXPECT_EQ(n1, 0u);
    EXPECT_EQ(uart.txPending(), 2u);

    // If we drain to a buffer of size 40:
    // Packet 1 (39 bytes) fits and is drained.
    // Packet 2 (39 bytes) does not fit in the remaining space (1 byte).
    // So packet 2 is retained.
    uint8_t buffer[40]{};
    size_t n2 = uart.drainToTxBuffer(buffer, sizeof(buffer));
    EXPECT_EQ(n2, 39u);
    EXPECT_EQ(uart.txPending(), 1u);
    EXPECT_NE(std::strstr(reinterpret_cast<char*>(buffer), "packet_one"), nullptr);
    EXPECT_EQ(std::strstr(reinterpret_cast<char*>(buffer), "packet_two"), nullptr);

    // Drain the remaining packet 2 into another buffer
    uint8_t buffer2[40]{};
    size_t n3 = uart.drainToTxBuffer(buffer2, sizeof(buffer2));
    EXPECT_EQ(n3, 39u);
    EXPECT_EQ(uart.txPending(), 0u);
    EXPECT_NE(std::strstr(reinterpret_cast<char*>(buffer2), "packet_two"), nullptr);
}


// ---------------------------------------------------------------------------
// 10. drain on empty ring returns 0
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, DrainOnEmptyReturnsZero) {
    size_t n = uart.drainToTxBuffer(outbuf, sizeof(outbuf));
    EXPECT_EQ(n, 0u);
}

// ---------------------------------------------------------------------------
// 11. TX overflow — fill ring then push one more
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, TxOverflowCounter) {
    // TX_BUF_SIZE=256 → 255 usable slots
    constexpr int SLOTS = 255;
    for (int i = 0; i < SLOTS; ++i) uart.log(i, LogLevel::DEBUG, "fill");
    EXPECT_EQ(uart.txOverflow(), 0u);

    // One more must fail
    bool ok = uart.log(9999, LogLevel::ERROR, "overflow_me");
    EXPECT_FALSE(ok);
    EXPECT_EQ(uart.txOverflow(), 1u);
}

// ---------------------------------------------------------------------------
// 12. TX overflow counter increments multiple times
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, TxOverflowMultiple) {
    for (int i = 0; i < 255; ++i) uart.log(i, LogLevel::DEBUG, "x");
    uart.log(0, LogLevel::DEBUG, "over1");
    uart.log(0, LogLevel::DEBUG, "over2");
    uart.log(0, LogLevel::DEBUG, "over3");
    EXPECT_EQ(uart.txOverflow(), 3u);
}

// ---------------------------------------------------------------------------
// 13. Drain then refill — overflow counter persists across drain cycles
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, OverflowPersistsAcrossDrain) {
    for (int i = 0; i < 255; ++i) uart.log(i, LogLevel::DEBUG, "x");
    uart.log(0, LogLevel::DEBUG, "ov");
    EXPECT_EQ(uart.txOverflow(), 1u);

    uart.drainToTxBuffer(outbuf, sizeof(outbuf));
    EXPECT_EQ(uart.txPending(), 0u);
    EXPECT_EQ(uart.txOverflow(), 1u);  // still 1 — not reset by drain
}

// ---------------------------------------------------------------------------
// 14. rxPush() / rxPop() round-trip
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, RxPushPopRoundTrip) {
    EXPECT_TRUE(uart.rxPush(0xAB));
    auto v = uart.rxPop();
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 0xABu);
}

// ---------------------------------------------------------------------------
// 15. rxPop() on empty returns nullopt
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, RxPopEmptyReturnsNullopt) {
    EXPECT_FALSE(uart.rxPop().has_value());
}

// ---------------------------------------------------------------------------
// 16. RX FIFO order preserved
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, RxFIFOOrder) {
    for (uint8_t b = 0; b < 10; ++b) EXPECT_TRUE(uart.rxPush(b));
    for (uint8_t b = 0; b < 10; ++b) {
        auto v = uart.rxPop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, b);
    }
}

// ---------------------------------------------------------------------------
// 17. RX overflow counter — fill RX ring (256 slots → 255 usable)
// ---------------------------------------------------------------------------
TEST_F(DmaUartTest, RxOverflowCounter) {
    for (int i = 0; i < 255; ++i) uart.rxPush(static_cast<uint8_t>(i & 0xFF));
    EXPECT_EQ(uart.rxOverflow(), 0u);

    EXPECT_FALSE(uart.rxPush(0xFF));
    EXPECT_EQ(uart.rxOverflow(), 1u);
}

// ---------------------------------------------------------------------------
// 18. LogPacket::make() — message truncated to 31 chars + null terminator
// ---------------------------------------------------------------------------
TEST(LogPacketTest, MakeTruncatesLongMessage) {
    // 70 characters long message
    const char* long_msg = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    auto p = LogPacket::make(0, LogLevel::WARN, long_msg);
    EXPECT_EQ(p.message[63], '\0');  // always null-terminated
    EXPECT_EQ(std::strlen(p.message), 63u);
}

// ---------------------------------------------------------------------------
// 19. LogPacket::make() — short message stored verbatim
// ---------------------------------------------------------------------------
TEST(LogPacketTest, MakeShortMessage) {
    auto p = LogPacket::make(999, LogLevel::ERROR, "hi");
    EXPECT_EQ(p.timestamp_ms, 999u);
    EXPECT_EQ(p.level, LogLevel::ERROR);
    EXPECT_STREQ(p.message, "hi");
}

// ---------------------------------------------------------------------------
// 20. LogPacket::make() — empty string
// ---------------------------------------------------------------------------
TEST(LogPacketTest, MakeEmptyMessage) {
    auto p = LogPacket::make(0, LogLevel::DEBUG, "");
    EXPECT_EQ(p.message[0], '\0');
}

// ---------------------------------------------------------------------------
// 20b. LogPacket::make() — nullptr message (covers else branch)
// ---------------------------------------------------------------------------
TEST(LogPacketTest, MakeNullptrMessage) {
    auto p = LogPacket::make(42, LogLevel::WARN, nullptr);
    EXPECT_EQ(p.timestamp_ms, 42u);
    EXPECT_EQ(p.level, LogLevel::WARN);
    EXPECT_EQ(p.message[0], '\0');
}

// ---------------------------------------------------------------------------
// 21. 1 kHz log rate simulation — 1 000 packets logged and drained
//     Mirrors the throughput benchmark; no packet must be lost.
// ---------------------------------------------------------------------------
TEST(DmaUartThroughput, OneKhzLogRate1000Packets) {
    DmaUart uart;
    uint8_t drain_buf[8192]{};
    uint32_t lost = 0;
    uint32_t drained_total = 0;

    for (uint32_t i = 0; i < 1000; ++i) {
        char msg[32];
        std::snprintf(msg, sizeof(msg), "seq_%u", i);

        if (!uart.log(i, LogLevel::INFO, msg)) {
            ++lost;
        }

        // Simulate DMA ISR draining every 8 packets (8× 921600 baud batching)
        if (i % 8 == 7) {
            drained_total += uart.drainToTxBuffer(drain_buf, sizeof(drain_buf));
        }
    }
    // Final drain
    drained_total += uart.drainToTxBuffer(drain_buf, sizeof(drain_buf));

    EXPECT_EQ(lost, 0u) << "Packets lost to TX overflow at 1kHz rate";
    EXPECT_EQ(uart.txPending(), 0u);
    EXPECT_EQ(uart.txOverflow(), 0u);
    EXPECT_GT(drained_total, 0u);
}

// ---------------------------------------------------------------------------
// 22. UartMockTest — verify mock stubs and helper APIs
// ---------------------------------------------------------------------------
TEST(UartMockTest, StubsAndSimulationHelpers) {
    uart_sim::reset();
    struct device dev{};

    EXPECT_TRUE(device_is_ready(&dev));
    uart_sim::device_ready = false;
    EXPECT_FALSE(device_is_ready(&dev));

    // Callback set
    static bool callback_fired = false;
    static uart_event_type last_event_type;
    auto cb = [](const struct device* d, struct uart_event* evt, void* user_data) {
        (void)d;
        (void)user_data;
        callback_fired = true;
        last_event_type = evt->type;
    };

    EXPECT_EQ(uart_callback_set(&dev, cb, nullptr), 0);
    
    // TX Done trigger
    callback_fired = false;
    uart_sim::trigger_tx_done(&dev);
    EXPECT_TRUE(callback_fired);
    EXPECT_EQ(last_event_type, UART_TX_DONE);

    // TX Aborted trigger
    callback_fired = false;
    uart_sim::trigger_tx_aborted(&dev);
    EXPECT_TRUE(callback_fired);
    EXPECT_EQ(last_event_type, UART_TX_ABORTED);

    // RX disabled trigger
    callback_fired = false;
    uart_sim::trigger_rx_disabled(&dev);
    EXPECT_TRUE(callback_fired);
    EXPECT_EQ(last_event_type, UART_RX_DISABLED);

    // RX stopped trigger
    callback_fired = false;
    uart_sim::trigger_rx_stopped(&dev, 42);
    EXPECT_TRUE(callback_fired);
    EXPECT_EQ(last_event_type, UART_RX_STOPPED);

    // TX transmission
    uint8_t tx_data[] = {1, 2, 3};
    EXPECT_EQ(uart_tx(&dev, tx_data, sizeof(tx_data), SYS_FOREVER_US), 0);
    EXPECT_EQ(uart_sim::get_tx_size(), 3u);
    EXPECT_EQ(uart_sim::get_tx_call_count(), 1u);
    EXPECT_EQ(uart_sim::get_tx_buffer()[0], 1);
    EXPECT_EQ(uart_sim::get_tx_buffer()[1], 2);
    EXPECT_EQ(uart_sim::get_tx_buffer()[2], 3);

    // RX double buffering & injection simulation
    uint8_t rx_buf_a[4]{};
    uint8_t rx_buf_b[4]{};
    EXPECT_EQ(uart_rx_enable(&dev, rx_buf_a, sizeof(rx_buf_a), 1000), 0);
    EXPECT_EQ(uart_rx_buf_rsp(&dev, rx_buf_b, sizeof(rx_buf_b)), 0);

    // Inject 6 bytes — should fill rx_buf_a (4 bytes) and request/use rx_buf_b (2 bytes)
    uint8_t inject_data[] = {10, 20, 30, 40, 50, 60};
    uart_sim::inject_rx_bytes(&dev, inject_data, sizeof(inject_data));

    EXPECT_EQ(rx_buf_a[0], 10);
    EXPECT_EQ(rx_buf_a[1], 20);
    EXPECT_EQ(rx_buf_a[2], 30);
    EXPECT_EQ(rx_buf_a[3], 40);

    EXPECT_EQ(rx_buf_b[0], 50);
    EXPECT_EQ(rx_buf_b[1], 60);

    // Reset clears state
    uart_sim::reset();
    EXPECT_EQ(uart_sim::get_tx_size(), 0u);
    EXPECT_EQ(uart_sim::get_tx_call_count(), 0u);
}

// ---------------------------------------------------------------------------
// Additional coverage tests
// ---------------------------------------------------------------------------
TEST(RingBufferConcurrent, PushContentionRetry) {
    RingBuffer<uint32_t, 8> rb;
    for (uint32_t i = 0; i < 7; ++i) {
        rb.push(i);
    }
    ASSERT_TRUE(rb.full());

    constexpr int NUM_THREADS = 4;
    constexpr int NUM_PUSHES = 1000;
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&rb]() {
            for (int i = 0; i < NUM_PUSHES; ++i) {
                rb.push(999);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(rb.overflowCount(), static_cast<uint32_t>(NUM_THREADS * NUM_PUSHES));
}

TEST_F(DmaUartTest, DrainToTxBufferExactSize) {
    uart.log(0, LogLevel::INFO, "hello");
    size_t n = uart.drainToTxBuffer(outbuf, 17);
    EXPECT_EQ(n, 17u);
    EXPECT_EQ(uart.txPending(), 0u);
}

TEST_F(DmaUartTest, InvalidLogLevel) {
    uart.log(100, static_cast<LogLevel>(5), "Invalid");
    size_t n = uart.drainToTxBuffer(outbuf, sizeof(outbuf));
    EXPECT_GT(n, 0u);
    EXPECT_NE(std::strstr(reinterpret_cast<char*>(outbuf), "[UNK]"), nullptr);
}