// =============================================================================
// GPIO — DigitalInput / DigitalOutput / ButtonEventBus implementations
// Target: NUCLEO-F446RE   Zephyr RTOS  (compatible with Zephyr 4.x)
// =============================================================================
#include "digital_io.hpp"

#ifdef __ZEPHYR__
  #include <zephyr/drivers/gpio.h>
  #include <zephyr/kernel.h>
#else
  #include "zephyr_gpio_mock.hpp"
#endif

// ---------------------------------------------------------------------------
// DigitalInput
// ---------------------------------------------------------------------------
DigitalInput::DigitalInput(const gpio_dt_spec& spec) : spec_(spec) {
    gpio_pin_configure_dt(&spec_, GPIO_INPUT);
}

GpioState DigitalInput::read() const {
    return gpio_pin_get_dt(&spec_) ? GpioState::High : GpioState::Low;
}

// ---------------------------------------------------------------------------
// DigitalOutput
// ---------------------------------------------------------------------------
DigitalOutput::DigitalOutput(const gpio_dt_spec& spec, GpioState initial)
    : spec_(spec)
{
    gpio_pin_configure_dt(&spec_, GPIO_OUTPUT);
    write(initial);
}

void DigitalOutput::write(GpioState s) {
    gpio_pin_set_dt(&spec_, static_cast<int>(s));
    state_ = s;
}

void DigitalOutput::toggle() {
    gpio_pin_toggle_dt(&spec_);
    state_ = (state_ == GpioState::Low) ? GpioState::High : GpioState::Low;
}

GpioState DigitalOutput::state() const {
    return state_;
}

// ---------------------------------------------------------------------------
// ButtonEventBus
// ---------------------------------------------------------------------------
ButtonEventBus::ButtonEventBus(const gpio_dt_spec& btn_spec)
    : button_(btn_spec), listener_count_(0)
{
    // Zero the listener array — plain aggregate, no std::array::fill needed
    for (size_t i = 0; i < MAX_LISTENERS; ++i) {
        listeners_[i] = nullptr;
    }

    // Store self so the static timer callback can recover `this`
    debounce_timer_.user_data = this;
    k_timer_init(&debounce_timer_, &ButtonEventBus::timerExpiry, nullptr);
}

bool ButtonEventBus::subscribe(IButtonListener* l) {
    if (!l || listener_count_ >= MAX_LISTENERS) {
        return false;
    }
    listeners_[listener_count_++] = l;
    return true;
}

bool ButtonEventBus::unsubscribe(IButtonListener* l) {
    for (size_t i = 0; i < listener_count_; ++i) {
        if (listeners_[i] == l) {
            listeners_[i] = listeners_[--listener_count_];
            listeners_[listener_count_] = nullptr;
            return true;
        }
    }
    return false;
}

void ButtonEventBus::notify(ButtonEvent ev) {
    for (size_t i = 0; i < listener_count_; ++i) {
        if (listeners_[i]) {
            listeners_[i]->onButtonEvent(ev);
        }
    }
}

// Arms the 10 ms debounce one-shot timer (called from GPIO ISR)
void ButtonEventBus::onRawInterrupt() {
    k_timer_start(&debounce_timer_, K_MSEC(DEBOUNCE_MS), K_NO_WAIT);
}

// Test helper — immediately fires the debounce expiry (no real time passes)
void ButtonEventBus::fireDebounceTimer() {
#ifdef __ZEPHYR__
    // In real Zephyr, this function is a no-op as it is only a test helper
    (void)debounce_timer_;
#else
    k_timer_fire(&debounce_timer_);
#endif
}

size_t ButtonEventBus::listenerCount() const {
    return listener_count_;
}

void ButtonEventBus::timerExpiry(k_timer* t) {
    
    auto* self = static_cast<ButtonEventBus*>(t->user_data);
    if (self) {
        self->notify(ButtonEvent::Pressed);
    }
}

IButtonListener::~IButtonListener() = default;