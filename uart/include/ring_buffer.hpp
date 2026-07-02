#pragma once
// =============================================================================
// UART — lock-free SPSC RingBuffer<T,N>  +  DMA UART driver
// C++20 template.  Single-Producer Single-Consumer, no heap, no mutex.
// Atomic head/tail for ISR safety on Cortex-M4 (single-core).
// =============================================================================
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>

// ---------------------------------------------------------------------------
// RingBuffer<T,N> — lock-free SPSC
// N must be a power-of-two for the bitmask optimisation.
// ---------------------------------------------------------------------------
template<typename T, size_t N>
    requires (N > 1 && (N & (N - 1)) == 0)   // C++20 concept-lite requires
class RingBuffer {
public:
    // Push from producer side. Returns false and increments saturating overflow counter on full.
    bool push(const T& val) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next = (head + 1) & MASK;
        if (next == tail_.load(std::memory_order_acquire)) {
            uint32_t current = overflow_count_.load(std::memory_order_relaxed);
            while (current < UINT32_MAX && 
                   !overflow_count_.compare_exchange_weak(current, current + 1,
                                                          std::memory_order_relaxed,
                                                          std::memory_order_relaxed)) {
                // retry if another thread modified it, until saturated or successful
            }
            return false;   // full — saturate
        }
        buf_[head] = val;
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Pop from consumer side. Returns nullopt when empty.
    std::optional<T> pop() {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return std::nullopt;  // empty
        T val = buf_[tail];
        tail_.store((tail + 1) & MASK, std::memory_order_release);
        return val;
    }

    // Peek from consumer side. Returns pointer to value or nullptr when empty.
    [[nodiscard]] const T* peek() const {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return nullptr;  // empty
        return &buf_[tail];
    }

    [[nodiscard]] bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool full() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return ((h + 1) & MASK) == t;
    }
    [[nodiscard]] size_t size() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return (h - t) & MASK;
    }
    [[nodiscard]] uint32_t overflowCount() const {
        return overflow_count_.load(std::memory_order_relaxed);
    }
    void resetOverflow() { overflow_count_.store(0, std::memory_order_relaxed); }
    void reset() {
        head_.store(0, std::memory_order_release);
        tail_.store(0, std::memory_order_release);
        resetOverflow();
    }

private:
    static constexpr size_t MASK = N - 1;
    std::array<T, N>         buf_{};
    std::atomic<size_t>      head_{0};
    std::atomic<size_t>      tail_{0};  
    std::atomic<uint32_t>    overflow_count_{0};
};

