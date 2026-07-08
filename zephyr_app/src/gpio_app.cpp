#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "digital_io.hpp"
#include "listeners.hpp"

LOG_MODULE_REGISTER(gpio_app, LOG_LEVEL_INF);
// ── DT aliases (defined by the NUCLEO-F446RE board DTS) ──────────────────────
// sw0  → PC13 (B1 user button, active LOW, internal pull-up)
// led0 → PA5  (LD2 green LED)
static const struct gpio_dt_spec btn_spec =
    GPIO_DT_SPEC_GET(DT_ALIAS(sw0),  gpios);
static const struct gpio_dt_spec led0_spec =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1_spec =
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// ── Static module objects (no heap) ──────────────────────────────────────────
struct GpioAppContext {
    ButtonEventBus    bus;
    LedToggleListener led0_listener;
    LedToggleListener led1_listener;
    UartLogListener   log_listener;
    struct k_timer    debounce_timer;

    GpioAppContext(const gpio_dt_spec& btn, const gpio_dt_spec& led0, const gpio_dt_spec& led1)
        : bus(btn), led0_listener(led0), led1_listener(led1), log_listener() {}
};

// Single static instance — safe because gpio_app_init() is called once
// from main() before any ISR fires.
static GpioAppContext* g_ctx = nullptr;

// ── Debounce timer expiry (workqueue / ISR context) ───────────────────────────
// Fires 10 ms after the last GPIO edge. Reads stable pin state, notifies bus.
static void debounce_expiry(struct k_timer* t)
{
    // CONTAINER_OF: recover GpioAppContext* from embedded k_timer member
    GpioAppContext* ctx =
        CONTAINER_OF(t, GpioAppContext, debounce_timer);

    // B1 is active LOW — pin LOW means physically pressed
    bool pressed = (gpio_pin_get_dt(&btn_spec) == 0);

    LOG_DBG("Debounce expired: pin=%s", pressed ? "PRESSED" : "RELEASED");
    ctx->bus.notify(
    pressed ? ButtonEvent::Pressed
            : ButtonEvent::Released);
}

// ── GPIO ISR callback (fires on both edges) ───────────────────────────────────
static struct gpio_callback button_cb_data;

static void button_isr(const struct device* dev,
                        struct gpio_callback* cb,
                        uint32_t pins_mask)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins_mask);

    if (g_ctx) {
        // Restart debounce window — any subsequent edge within 10 ms resets it
        k_timer_start(&g_ctx->debounce_timer,
                      K_MSEC(ButtonEventBus::DEBOUNCE_MS), K_NO_WAIT);
    }
}

// ── Public init ──────────────────────────────────────────────────────────────
void gpio_app_init(void)
{
    LOG_INF("GPIO init: Button=PC13(sw0), OnboardGreenLED=PA5(led0), ExternalYellowLED=PB1(led1)");

    // Validate DT devices are ready
    if (!gpio_is_ready_dt(&btn_spec)) {
        LOG_ERR("Button GPIO device not ready");
        return;
    }
    if (!gpio_is_ready_dt(&led0_spec)) {
        LOG_ERR("LED0 GPIO device not ready");
        return;
    }
    if (!gpio_is_ready_dt(&led1_spec)) {
        LOG_ERR("LED1 GPIO device not ready");
        return;
    }

    // Allocate context (static — lives for the lifetime of the app)
    static GpioAppContext ctx(btn_spec, led0_spec, led1_spec);
    g_ctx = &ctx;

    // Subscribe observers to the bus
    ctx.bus.subscribe(&ctx.led0_listener);
    ctx.bus.subscribe(&ctx.led1_listener);
    ctx.bus.subscribe(&ctx.log_listener);

    // Configure button GPIO — input with internal pull-up (B1 is active LOW)
    gpio_pin_configure_dt(&btn_spec, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&btn_spec, GPIO_INT_EDGE_BOTH);

    // Register ISR callback
    gpio_init_callback(&button_cb_data, button_isr, BIT(btn_spec.pin));
    gpio_add_callback(btn_spec.port, &button_cb_data);

    // Init debounce timer (one-shot, no auto-reload)
    k_timer_init(&ctx.debounce_timer, debounce_expiry, nullptr);

    LOG_INF("GPIO observer ready — subscribers: %zu", ctx.bus.listenerCount());
}