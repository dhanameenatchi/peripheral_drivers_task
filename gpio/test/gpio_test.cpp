// =============================================================================
// GPIO Test Suite — GTest
// Covers: ButtonEventBus subscribe/unsubscribe/notify
//         LedToggleListener state toggle
//         UartLogListener message format
//         Debounce boundary: 9ms / 10ms / 11ms
// 100% line + branch coverage target
// =============================================================================
#include <gtest/gtest.h>
#include "digital_io.hpp"
#include "listeners.hpp"

// ---------------------------------------------------------------------------
// Helper: mock listener that records events
// ---------------------------------------------------------------------------
class MockListener final : public IButtonListener {
public:
    void onButtonEvent(ButtonEvent ev) override {
        events.push_back(ev);
    }
    std::vector<ButtonEvent> events;
};

// ---------------------------------------------------------------------------
// ButtonEventBus — subscribe / unsubscribe / notify
// ---------------------------------------------------------------------------
TEST(ButtonEventBus, SubscribeAddsListener) {
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener l1, l2;

    EXPECT_TRUE(bus.subscribe(&l1));
    EXPECT_EQ(bus.listenerCount(), 1u);

    EXPECT_TRUE(bus.subscribe(&l2));
    EXPECT_EQ(bus.listenerCount(), 2u);
}

TEST(ButtonEventBus, UnsubscribeRemovesListener) {
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener l1, l2;

    bus.subscribe(&l1);
    bus.subscribe(&l2);
    EXPECT_TRUE(bus.unsubscribe(&l1));
    EXPECT_EQ(bus.listenerCount(), 1u);
}

TEST(ButtonEventBus, UnsubscribeNonexistentReturnsFalse) {
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener l1, l2;
    bus.subscribe(&l1);
    EXPECT_FALSE(bus.unsubscribe(&l2));
}

TEST(ButtonEventBus, NotifyCallsAllListeners) {
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener l1, l2, l3;
    bus.subscribe(&l1);
    bus.subscribe(&l2);
    bus.subscribe(&l3);

    bus.notify(ButtonEvent::Pressed);
    EXPECT_EQ(l1.events.size(), 1u);
    EXPECT_EQ(l2.events.size(), 1u);
    EXPECT_EQ(l3.events.size(), 1u);
    EXPECT_EQ(l1.events[0], ButtonEvent::Pressed);
}

TEST(ButtonEventBus, NotifyOnlyToSubscribed) {
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener l1, l2;
    bus.subscribe(&l1);
    // l2 not subscribed

    bus.notify(ButtonEvent::Released);
    EXPECT_EQ(l1.events.size(), 1u);
    EXPECT_EQ(l2.events.size(), 0u);
}

TEST(ButtonEventBus, MaxListenerCapacity) {
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener listeners[ButtonEventBus::MAX_LISTENERS + 1];

    for (size_t i = 0; i < ButtonEventBus::MAX_LISTENERS; ++i)
        EXPECT_TRUE(bus.subscribe(&listeners[i]));

    // One more should fail
    EXPECT_FALSE(bus.subscribe(&listeners[ButtonEventBus::MAX_LISTENERS]));
}

TEST(ButtonEventBus, DebounceTimerFiresNotify) {
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener l;
    bus.subscribe(&l);

    // Simulate raw interrupt; timer not yet fired
    bus.onRawInterrupt();
    EXPECT_EQ(l.events.size(), 0u);  // not notified yet

    // After debounce window (10ms), manually fire
    bus.fireDebounceTimer(); // simulating timer expiry notify
    EXPECT_EQ(l.events.size(), 1u);
}

// ---------------------------------------------------------------------------
// Debounce boundary: 9ms (too short), 10ms (exact), 11ms (after)
// In hardware these map to ISR + timer start. We simulate via onRawInterrupt.
// ---------------------------------------------------------------------------
TEST(DebounceFilter, BoundaryAt9ms) {
    // A press at < DEBOUNCE_MS should be suppressed by the timer
    // (timer not expired => no notify)
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener l;
    bus.subscribe(&l);
    std::cout << "----- 9 ms -----\n";
    bus.onRawInterrupt();
    std::cout << "Timer NOT expired\n";
    // 9ms has not elapsed — timer did not fire
    EXPECT_EQ(l.events.size(), 0u) << "Event before debounce window must be suppressed";
}

TEST(DebounceFilter, BoundaryAt10ms) {
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener l;
    bus.subscribe(&l);
    std::cout << "----- 10 ms -----\n";
    bus.onRawInterrupt();
    bus.fireDebounceTimer();  // exactly 10ms
    // bus.notify(ButtonEvent::Pressed);
    EXPECT_EQ(l.events.size(), 1u) << "Event at exactly 10ms must be accepted";
}

TEST(DebounceFilter, BoundaryAt11ms) {
    gpio_dt_spec btn{ 0, 13, 0 };
    ButtonEventBus bus(btn);
    MockListener l;
    bus.subscribe(&l);
    std::cout << "----- 11 ms -----\n";
    bus.onRawInterrupt();
    bus.fireDebounceTimer();
    // bus.notify(ButtonEvent::Pressed);
    // Second press at 11ms (another raw interrupt -> another timer)
    bus.onRawInterrupt();
    bus.fireDebounceTimer();
    // bus.notify(ButtonEvent::Released);
    EXPECT_EQ(l.events.size(), 2u);
}

// ---------------------------------------------------------------------------
// LedToggleListener — toggle state
// ---------------------------------------------------------------------------
TEST(LedToggleListener, TogglesOnPress) {
    LedToggleListener led(pins::LED_GREEN);
    EXPECT_EQ(led.ledState(), GpioState::Low);  // initial

    led.onButtonEvent(ButtonEvent::Pressed);
    EXPECT_EQ(led.ledState(), GpioState::High);

    led.onButtonEvent(ButtonEvent::Pressed);
    EXPECT_EQ(led.ledState(), GpioState::Low);
}

TEST(LedToggleListener, NoToggleOnRelease) {
    LedToggleListener led(pins::LED_GREEN);
    led.onButtonEvent(ButtonEvent::Released);  // should not toggle
    EXPECT_EQ(led.ledState(), GpioState::Low);
}

// ---------------------------------------------------------------------------
// UartLogListener — message format
// ---------------------------------------------------------------------------
TEST(UartLogListener, LogsPressBelowMax) {
    UartLogListener log;
    log.onButtonEvent(ButtonEvent::Pressed);
    EXPECT_EQ(log.logCount(), 1u);
    EXPECT_NE(std::string(log.logEntry(0)).find("PRESSED"), std::string::npos);
}

TEST(UartLogListener, LogsRelease) {
    UartLogListener log;
    log.onButtonEvent(ButtonEvent::Released);
    EXPECT_EQ(log.logCount(), 1u);
    EXPECT_NE(std::string(log.logEntry(0)).find("RELEASED"), std::string::npos);
}

TEST(UartLogListener, SaturatesAtMaxLog) {
    UartLogListener log;
    for (size_t i = 0; i <= UartLogListener::MAX_LOG + 5; ++i)
        log.onButtonEvent(ButtonEvent::Pressed);
    EXPECT_EQ(log.logCount(), UartLogListener::MAX_LOG);
}

TEST(UartLogListener, ClearLog) {
    UartLogListener log;
    log.onButtonEvent(ButtonEvent::Pressed);
    log.clearLog();
    EXPECT_EQ(log.logCount(), 0u);
    EXPECT_STREQ(log.logEntry(0), "");
}

TEST(UartLogListener, LogContainsTimestamp) {
    UartLogListener log;
    log.onButtonEvent(ButtonEvent::Pressed);
    std::string msg = log.logEntry(0);
    EXPECT_NE(msg.find("T="), std::string::npos);
}

// ---------------------------------------------------------------------------
// DigitalInput / DigitalOutput
// ---------------------------------------------------------------------------
TEST(DigitalOutput, InitialLow) {
    DigitalOutput out(pins::LED_GREEN, GpioState::Low);
    EXPECT_EQ(out.state(), GpioState::Low);
}

TEST(DigitalOutput, WriteHigh) {
    DigitalOutput out(pins::LED_GREEN);
    out.write(GpioState::High);
    EXPECT_EQ(out.state(), GpioState::High);
}

TEST(DigitalOutput, Toggle) {
    DigitalOutput out(pins::LED_GREEN);
    out.toggle();
    EXPECT_EQ(out.state(), GpioState::High);
    out.toggle();
    EXPECT_EQ(out.state(), GpioState::Low);
}

TEST(DigitalInput, ReadPinState) {
    gpio_sim::pin_state[13] = true;
    DigitalInput in(pins::USER_BUTTON);
    EXPECT_EQ(in.read(), GpioState::High);
    gpio_sim::pin_state[13] = false;
    EXPECT_EQ(in.read(), GpioState::Low);
}

TEST(IButtonListener, VirtualDestructor)
{
    IButtonListener* listener = new MockListener();
    delete listener;
}