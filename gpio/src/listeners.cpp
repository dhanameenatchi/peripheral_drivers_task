// =============================================================================
// Concrete Observer implementations — LedToggleListener / UartLogListener
// Target: NUCLEO-F446RE   Zephyr RTOS  (compatible with Zephyr 4.x)
// =============================================================================
#include "listeners.hpp"
#include <stdio.h>
#include <string.h>

#ifdef __ZEPHYR__
  #include <zephyr/logging/log.h>
  LOG_MODULE_DECLARE(gpio_app);
#endif

// ---------------------------------------------------------------------------
// LedToggleListener
// ---------------------------------------------------------------------------
LedToggleListener::LedToggleListener(const gpio_dt_spec& led_spec)
    : led_(led_spec) {}

void LedToggleListener::onButtonEvent(ButtonEvent ev) {
    if (ev == ButtonEvent::Pressed) {
        led_.toggle();
    }
}

GpioState LedToggleListener::ledState() const {
    return led_.state();
}

// ---------------------------------------------------------------------------
// UartLogListener
// ---------------------------------------------------------------------------
void UartLogListener::onButtonEvent(ButtonEvent ev) {
    uint32_t ts = k_uptime_get_32();
    const char* evStr = (ev == ButtonEvent::Pressed) ? "PRESSED" : "RELEASED";

#ifdef __ZEPHYR__
    LOG_INF("[T=%u ms] BUTTON %s", ts, evStr);
#else
    printf("[T=%u ms] BUTTON %s\n", (unsigned int)ts, evStr);
#endif

    if (log_count_ < MAX_LOG) {
        snprintf(log_[log_count_], MSG_LEN, "[T=%ums] BUTTON %s", ts, evStr);
        ++log_count_;
    }
}

size_t UartLogListener::logCount() const {
    return log_count_;
}

const char* UartLogListener::logEntry(size_t i) const {
    if (i >= log_count_) {
        return "";
    }
    return log_[i];
}

void UartLogListener::clearLog() {
    log_count_ = 0;
}
