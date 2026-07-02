// =============================================================================
// bench_uart.cpp — Throughput benchmark: CPU-blocking vs DMA at 1 kHz
// Measures: cycles / time per 1 000 log calls for each strategy.
// Also performs overflow injection test.
// Build standalone: g++ -std=c++20 -O2 -o bench bench_uart.cpp
// =============================================================================
#include "dma_uart.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>

using Clock = std::chrono::steady_clock;
using us    = std::chrono::microseconds;

// ---------------------------------------------------------------------------
// CPU-blocking UART stub — simulates synchronous byte-by-byte transmission
// at 921600 baud.  Each byte takes ~10.85 µs (1/92160 s).
// For host benchmarking we model this as a busy-wait in software.
// ---------------------------------------------------------------------------
namespace blocking {

static constexpr double BAUD        = 921600.0;
static constexpr double BITS_PER_CH = 10.0;   // 1 start + 8 data + 1 stop
static constexpr double US_PER_CHAR = (BITS_PER_CH / BAUD) * 1e6;  // ~10.85 µs

// Simulated blocking transmit: spin for (len × US_PER_CHAR) microseconds
static void uart_write_blocking(const char* buf, size_t len) {
    auto delay = std::chrono::duration<double, std::micro>(len * US_PER_CHAR);
    auto end   = Clock::now() + std::chrono::duration_cast<Clock::duration>(delay);
    while (Clock::now() < end) { /* spin — models CPU stuck waiting for UART shift register */ }
}

// Format + transmit a log packet synchronously (CPU occupied the whole time)
static void log_blocking(uint32_t ts, LogLevel lvl, const char* msg) {
    static constexpr const char* LEVEL_STR[] = {"DBG","INF","WRN","ERR"};
    char line[80];
    int n = std::snprintf(line, sizeof(line), "T=%u [%s] %s\r\n",
                          ts, LEVEL_STR[static_cast<uint8_t>(lvl)], msg);
    if (n > 0) uart_write_blocking(line, static_cast<size_t>(n));
}

} // namespace blocking

// ---------------------------------------------------------------------------
// Result type
// ---------------------------------------------------------------------------
struct BenchResult {
    const char* name;
    double      total_us;
    double      per_call_us;
    uint32_t    packets;
    uint32_t    overflows;
    bool        cpu_free;   // true if CPU was not blocked during TX
};

static void print_result(const BenchResult& r) {
    std::printf("%-28s  packets=%4u  overflows=%u  total=%8.1f µs  per_call=%6.2f µs  CPU_free=%s\n",
        r.name, r.packets, r.overflows,
        r.total_us, r.per_call_us,
        r.cpu_free ? "YES" : "NO");
}

// ---------------------------------------------------------------------------
// Benchmark A — CPU-blocking 1 kHz (1 000 packets)
// ---------------------------------------------------------------------------
static BenchResult bench_blocking(uint32_t n_packets) {
    auto t0 = Clock::now();

    for (uint32_t i = 0; i < n_packets; ++i) {
        char msg[32];
        std::snprintf(msg, sizeof(msg), "seq_%u", i);
        blocking::log_blocking(i, LogLevel::INFO, msg);
    }

    auto t1 = Clock::now();
    double us_total = static_cast<double>(
        std::chrono::duration_cast<us>(t1 - t0).count());

    return { "CPU-blocking UART", us_total, us_total / n_packets,
             n_packets, 0, false };
}

// ---------------------------------------------------------------------------
// Benchmark B — DMA (ring-buffer enqueue only) 1 kHz (1 000 packets)
// The "DMA transfer" is simulated by a background thread draining the ring,
// models the DMA complete ISR calling drainToTxBuffer() asynchronously.
// ---------------------------------------------------------------------------
static BenchResult bench_dma(uint32_t n_packets) {
    static DmaUart uart;

    std::atomic<bool> stop_drain{false};
    std::atomic<bool> drain_running{false};
    static uint8_t    drain_buf[4096];

    // Background drain thread (simulates DMA ISR)
    std::thread drainer([&] {
        drain_running.store(true);
        while (!stop_drain.load(std::memory_order_relaxed)) {
            uart.drainToTxBuffer(drain_buf, sizeof(drain_buf));
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        uart.drainToTxBuffer(drain_buf, sizeof(drain_buf));  // final flush
    });

    // Wait for drainer to start
    while (!drain_running.load()) {}

    // Measure only the enqueue (push to ring buffer) cost — CPU cost of log()
    auto t0 = Clock::now();

    for (uint32_t i = 0; i < n_packets; ++i) {
        char msg[32];
        std::snprintf(msg, sizeof(msg), "seq_%u", i);
        uart.log(i, LogLevel::INFO, msg);
    }

    auto t1 = Clock::now();
    double us_total = static_cast<double>(
        std::chrono::duration_cast<us>(t1 - t0).count());

    stop_drain.store(true);
    drainer.join();

    uint32_t overflows = uart.txOverflow();

    return { "DMA UART (ring enqueue)", us_total, us_total / n_packets,
             n_packets, overflows, true };
}

// ---------------------------------------------------------------------------
// Overflow injection test — confirms saturating counter works under DMA path
// ---------------------------------------------------------------------------
static void test_overflow_injection() {
    std::printf("\n--- Overflow injection test ---\n");

    DmaUart uart;
    // Do NOT drain — let ring fill up
    constexpr int TOTAL = 300;   // TX_BUF_SIZE=256 → 255 slots → 45 overflows
    int success = 0, fail = 0;
    for (int i = 0; i < TOTAL; ++i) {
        if (uart.log(i, LogLevel::WARN, "overflow_inject")) ++success;
        else ++fail;
    }
    std::printf("  Pushed: %d   Succeeded: %d   Failed(overflow): %d\n",
                TOTAL, success, fail);
    std::printf("  txOverflow counter: %u\n", uart.txOverflow());
    std::printf("  txPending:          %zu\n", uart.txPending());

    bool pass = (fail > 0) && (uart.txOverflow() == static_cast<uint32_t>(fail));
    std::printf("  Result: %s\n", pass ? "PASS" : "FAIL");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    constexpr uint32_t N = 1000;

    std::printf("=== UART Throughput Benchmark — %u packets @ 921600 baud ===\n\n", N);

    BenchResult r_block = bench_blocking(N);
    BenchResult r_dma   = bench_dma(N);

    print_result(r_block);
    print_result(r_dma);

    double speedup = r_block.per_call_us / r_dma.per_call_us;
    std::printf("\nDMA enqueue is %.1fx faster per call (CPU free during TX)\n", speedup);
    std::printf("CPU-blocking total: %.1f ms  |  DMA enqueue total: %.1f ms\n",
                r_block.total_us / 1000.0, r_dma.total_us / 1000.0);

    test_overflow_injection();

    std::printf("\nBenchmark complete.\n");
    return 0;
}