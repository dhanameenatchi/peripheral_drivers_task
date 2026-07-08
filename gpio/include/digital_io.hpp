#pragma once

#ifdef __ZEPHYR__
  #include <zephyr/drivers/gpio.h>
  #include <zephyr/kernel.h>
#else
  #include "zephyr_gpio_mock.hpp"
#endif

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Pin mapping — constexpr, lives in flash not RAM
// .port = nullptr is valid: gpio_dt_spec::port is `const device*` (pointer)
// ---------------------------------------------------------------------------
namespace pins {
    constexpr gpio_dt_spec USER_BUTTON = { .port = nullptr, .pin = 13, .dt_flags = 0 };
    constexpr gpio_dt_spec LED_GREEN   = { .port = nullptr, .pin =  0, .dt_flags = 0 };
    constexpr gpio_dt_spec LED_YELLOW  = { .port = nullptr, .pin =  1, .dt_flags = 0 };
}

// ---------------------------------------------------------------------------
// GPIO state
// ---------------------------------------------------------------------------
enum class GpioState : uint8_t { Low = 0, High = 1 };

// ---------------------------------------------------------------------------
// DigitalInput — read-only GPIO pin
// ---------------------------------------------------------------------------
class DigitalInput {
public:
    explicit DigitalInput(const gpio_dt_spec& spec);

    [[nodiscard]] GpioState read() const;

private:
    gpio_dt_spec spec_;
};

// ---------------------------------------------------------------------------
// DigitalOutput — drive a GPIO pin
// ---------------------------------------------------------------------------
class DigitalOutput {
public:
    explicit DigitalOutput(const gpio_dt_spec& spec,
                           GpioState initial = GpioState::Low);

    void write(GpioState s);
    void toggle();

    [[nodiscard]] GpioState state() const;

private:
    gpio_dt_spec spec_;
    GpioState    state_ = GpioState::Low;
};

// ---------------------------------------------------------------------------
// Observer pattern interfaces
// ---------------------------------------------------------------------------
enum class ButtonEvent : uint8_t { Pressed, Released };

struct IButtonListener {
    virtual void onButtonEvent(ButtonEvent) = 0;
    virtual ~IButtonListener();
};

// ---------------------------------------------------------------------------
// ButtonEventBus — subscriber list + debounce via k_timer
//
// Zephyr 4.x change: replaced std::array<IButtonListener*, 8> with a plain
// C-style array IButtonListener* listeners_[MAX_LISTENERS].
// std::array requires <array> which is not available under -nostdinc++
// (Zephyr's default MINIMAL_LIBCPP mode).  A raw array is equivalent here:
// same size, same element type, no heap, and directly indexable.
//
// API preserved: subscribe / unsubscribe / notify / onRawInterrupt /
//                fireDebounceTimer / listenerCount — tests unchanged.
// ---------------------------------------------------------------------------
class ButtonEventBus {
public:
    static constexpr uint32_t DEBOUNCE_MS   = 10;
    static constexpr size_t   MAX_LISTENERS =  8;

    explicit ButtonEventBus(const gpio_dt_spec& btn_spec);

    bool subscribe(IButtonListener* l);
    bool unsubscribe(IButtonListener* l);
    void notify(ButtonEvent ev);

    // Arms the 10 ms debounce one-shot timer (called from GPIO ISR)
    void onRawInterrupt();

    // Test helper — immediately fires the debounce expiry (no real time passes)
    void fireDebounceTimer();

    [[nodiscard]] size_t listenerCount() const;

private:
    static void timerExpiry(k_timer* t);

    DigitalInput   button_;
    k_timer        debounce_timer_;
    // Plain array instead of std::array — no <array> header required
    IButtonListener* listeners_[MAX_LISTENERS];
    size_t           listener_count_;
};