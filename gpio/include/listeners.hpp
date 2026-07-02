#pragma once
// =============================================================================
// Concrete Observer implementations
// LedToggleListener — toggles an LED on each button press
// UartLogListener   — formats and records a timestamped log message
// =============================================================================
#include "digital_io.hpp"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// LedToggleListener
// ---------------------------------------------------------------------------
class LedToggleListener final : public IButtonListener {
public:
    explicit LedToggleListener(const gpio_dt_spec& led_spec);

    void onButtonEvent(ButtonEvent ev) override;

    [[nodiscard]] GpioState ledState() const;

private:
    DigitalOutput led_;
};

// ---------------------------------------------------------------------------
// UartLogListener
// Formats: "[T=<ms>] BUTTON <PRESSED|RELEASED>\r\n"
// Keeps last N log entries in a fixed-size 2D char array (no heap).
// ---------------------------------------------------------------------------
class UartLogListener final : public IButtonListener {
public:
    static constexpr size_t MAX_LOG = 32;
    static constexpr size_t MSG_LEN = 64;

    void onButtonEvent(ButtonEvent ev) override;

    [[nodiscard]] size_t logCount() const;

    [[nodiscard]] const char* logEntry(size_t i) const;

    void clearLog();

private:
    // Was: std::array<std::array<char, MSG_LEN>, MAX_LOG>
    // Now: plain 2D C array — no <array> include, identical memory layout
    char   log_[MAX_LOG][MSG_LEN]{};
    size_t log_count_ = 0;
};