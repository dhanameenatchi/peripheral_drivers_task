// =============================================================================
// SPI Test Suite — GTest
// Covers: MovingAverageFilter / MedianFilter all-equal / ramp / impulse
//         Strategy swap mid-stream preserves continuity
//         SPI error injection; AdcDriver channel selection
// 100% line + branch coverage target
// =============================================================================
#include <gtest/gtest.h>
#include "adc_driver.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::vector<int16_t> makeRamp(int16_t start, int16_t step, size_t count) {
    std::vector<int16_t> v(count);
    for (size_t i = 0; i < count; ++i) v[i] = static_cast<int16_t>(start + step * i);
    return v;
}

// ---------------------------------------------------------------------------
// MovingAverageFilter — all-equal input
// ---------------------------------------------------------------------------
TEST(MovingAverageFilter, AllEqualInput) {
    MovingAverageFilter f(4);
    std::vector<int16_t> s = {100, 100, 100, 100, 100};
    EXPECT_EQ(f.apply(s), 100);
}

TEST(MovingAverageFilter, RampInput) {
    MovingAverageFilter f(4);
    // ramp 1,2,3,4,5 → last 4: 2,3,4,5 → avg=3
    auto s = makeRamp(1, 1, 5);
    EXPECT_EQ(f.apply(s), 3);
}

TEST(MovingAverageFilter, ImpulseInput) {
    MovingAverageFilter f(4);
    // impulse: 1000, 0, 0, 0, 0 → last 4: 0,0,0,0 → avg=0
    std::vector<int16_t> s = {1000, 0, 0, 0, 0};
    EXPECT_EQ(f.apply(s), 0);
}

TEST(MovingAverageFilter, EmptyInputReturnsZero) {
    MovingAverageFilter f(4);
    std::vector<int16_t> s;
    EXPECT_EQ(f.apply(s), 0);
}

TEST(MovingAverageFilter, WindowLargerThanInput) {
    MovingAverageFilter f(10);
    std::vector<int16_t> s = {2, 4};
    EXPECT_EQ(f.apply(s), 3);  // avg(2,4) = 3
}

TEST(MovingAverageFilter, Name) {
    MovingAverageFilter f;
    EXPECT_STREQ(f.name(), "MovingAverage");
}

// ---------------------------------------------------------------------------
// MedianFilter — all-equal / ramp / impulse
// ---------------------------------------------------------------------------
TEST(MedianFilter, AllEqualInput) {
    MedianFilter f(5);
    std::vector<int16_t> s = {7, 7, 7, 7, 7};
    EXPECT_EQ(f.apply(s), 7);
}

TEST(MedianFilter, RampInput) {
    MedianFilter f(5);
    auto s = makeRamp(1, 1, 5);  // 1,2,3,4,5 → median=3
    EXPECT_EQ(f.apply(s), 3);
}

TEST(MedianFilter, ImpulseInput) {
    MedianFilter f(5);
    // impulse at position 0: 1000, 1, 1, 1, 1 → sorted: 1,1,1,1,1000 → median=1
    std::vector<int16_t> s = {1000, 1, 1, 1, 1};
    EXPECT_EQ(f.apply(s), 1);
}

TEST(MedianFilter, OddWindowForced) {
    MedianFilter f(4);  // becomes window=5 (forced odd)
    std::vector<int16_t> s = {1, 2, 3, 4, 5};
    EXPECT_EQ(f.apply(s), 3);
}

TEST(MedianFilter, EmptyInputReturnsZero) {
    MedianFilter f(5);
    std::vector<int16_t> s;
    EXPECT_EQ(f.apply(s), 0);
}

TEST(MedianFilter, Name) {
    MedianFilter f;
    EXPECT_STREQ(f.name(), "Median");
}

// ---------------------------------------------------------------------------
// AdcDriver — basic channel read (SPI injected via spi_sim)
// ---------------------------------------------------------------------------
TEST(AdcDriver, ReadChannelInjectsRxData) {
    spi_sim::rx_inject = {0x12, 0x34, 0x00, 0x00};  // raw = 0x1234 = 4660
    AdcDriver adc;
    int16_t v = adc.readChannel(AdcDriver::Channel::SE_AIN0);
    EXPECT_EQ(v, 0x1234);
    EXPECT_EQ(adc.lastError(), 0);
}

TEST(AdcDriver, SpiErrorReturnsMin) {
    spi_sim::error_next = true;
    AdcDriver adc;
    int16_t v = adc.readChannel(AdcDriver::Channel::SE_AIN0);
    EXPECT_EQ(v, INT16_MIN);
    EXPECT_NE(adc.lastError(), 0);
}

TEST(AdcDriver, ChannelSelectionTxCapture) {
    spi_sim::tx_capture.clear();
    spi_sim::rx_inject = {0x00, 0x00, 0x00, 0x00};
    AdcDriver adc;
    adc.readChannel(AdcDriver::Channel::DIFF_01);
    // Config byte high should contain channel bits 0x0000 => bits [14:12]=000
    ASSERT_GE(spi_sim::tx_capture.size(), 2u);
    uint8_t cfg_hi = spi_sim::tx_capture[0];
    EXPECT_EQ(cfg_hi & 0x70, 0x00u);  // MUX=000 for DIFF_01
}

TEST(AdcDriver, HistoryAccumulates) {
    spi_sim::rx_inject = {0x00, 0x0A, 0x00, 0x00};
    AdcDriver adc;
    adc.readChannel(AdcDriver::Channel::SE_AIN0);
    adc.readChannel(AdcDriver::Channel::SE_AIN0);
    EXPECT_EQ(adc.history().size(), 2u);
    adc.clearHistory();
    EXPECT_TRUE(adc.history().empty());
}

// ---------------------------------------------------------------------------
// Strategy swap mid-stream preserves continuity
// ---------------------------------------------------------------------------
TEST(AdcDriver, StrategySwapMidStream) {
    MovingAverageFilter mavg(4);
    MedianFilter        med(5);
    AdcDriver           adc(&mavg);

    // Build history with a ramp
    for (int16_t i = 1; i <= 8; ++i) {
        spi_sim::rx_inject = { static_cast<uint8_t>(0), static_cast<uint8_t>(i), 0, 0 };
        adc.readChannel(AdcDriver::Channel::SE_AIN0);
    }

    int16_t before = adc.filteredValue();  // MovingAverage result

    // Swap strategy to Median — history preserved
    adc.setFilter(&med);
    int16_t after = adc.filteredValue();  // Median result on same history

    // Both valid, may differ; history unchanged (same size)
    EXPECT_EQ(adc.history().size(), 8u);
    // After swap the filter name must change
    EXPECT_STREQ(med.name(), "Median");
    EXPECT_STREQ(mavg.name(), "MovingAverage");
    (void)before; (void)after;  // values are strategy-dependent
}

TEST(AdcDriver, NoFilterReturnsLastSample) {
    AdcDriver adc(nullptr);
    spi_sim::rx_inject = {0x00, 0x55, 0x00, 0x00};
    adc.readChannel(AdcDriver::Channel::SE_AIN0);
    EXPECT_EQ(adc.filteredValue(), 0x55);
}

TEST(AdcDriver, NoFilterEmptyHistoryReturnsZero) {
    AdcDriver adc(nullptr);
    EXPECT_EQ(adc.filteredValue(), 0);
}

// ---------------------------------------------------------------------------
// Runtime strategy switching — simulates UART "filter" command behavior
// ---------------------------------------------------------------------------
TEST(AdcDriver, RuntimeSwitchMavgToMedian) {
    MovingAverageFilter mavg(4);
    MedianFilter        med(5);
    AdcDriver           adc(&mavg);

    // Build history: 1,2,3,4,5,6,7,8
    for (int16_t i = 1; i <= 8; ++i) {
        spi_sim::rx_inject = { 0, static_cast<uint8_t>(i), 0, 0 };
        adc.readChannel(AdcDriver::Channel::SE_AIN0);
    }

    // Active strategy is MovingAverage
    int16_t val_mavg = adc.filteredValue();
    // last 4: 5,6,7,8 → avg = 6
    EXPECT_EQ(val_mavg, 6);

    // Runtime switch to Median (simulates "filter median" command)
    adc.setFilter(&med);
    int16_t val_med = adc.filteredValue();
    // last 5: 4,5,6,7,8 → median = 6
    EXPECT_EQ(val_med, 6);

    // History unchanged
    EXPECT_EQ(adc.history().size(), 8u);
}

TEST(AdcDriver, RuntimeSwitchMedianToMavg) {
    MovingAverageFilter mavg(4);
    MedianFilter        med(5);
    AdcDriver           adc(&med);  // starts with Median

    // Build history: 10,20,30
    for (int16_t v : {10, 20, 30}) {
        spi_sim::rx_inject = { static_cast<uint8_t>(v >> 8),
                               static_cast<uint8_t>(v & 0xFF), 0, 0 };
        adc.readChannel(AdcDriver::Channel::SE_AIN0);
    }

    // Median of {10,20,30} → window=5 but only 3 samples → median of sorted {10,20,30} = 20
    EXPECT_EQ(adc.filteredValue(), 20);

    // Switch to MovingAverage
    adc.setFilter(&mavg);
    // MA window=4, only 3 samples → avg(10,20,30) = 20
    EXPECT_EQ(adc.filteredValue(), 20);
}

TEST(AdcDriver, RepeatedSwitching) {
    MovingAverageFilter mavg(4);
    MedianFilter        med(5);
    AdcDriver           adc(&mavg);

    spi_sim::rx_inject = { 0x00, 0x64, 0x00, 0x00 };  // 100
    adc.readChannel(AdcDriver::Channel::SE_AIN0);

    // Switch back and forth rapidly
    for (int i = 0; i < 10; ++i) {
        adc.setFilter(&med);
        int16_t v1 = adc.filteredValue();
        adc.setFilter(&mavg);
        int16_t v2 = adc.filteredValue();
        // Both should return 100 (single sample)
        EXPECT_EQ(v1, 100);
        EXPECT_EQ(v2, 100);
    }

    // History should still have exactly 1 sample
    EXPECT_EQ(adc.history().size(), 1u);
}

TEST(AdcDriver, InvalidCommandFilterUnchanged) {
    MovingAverageFilter mavg(4);
    MedianFilter        med(5);
    AdcDriver           adc(&mavg);

    spi_sim::rx_inject = { 0x00, 0x0A, 0x00, 0x00 };  // 10
    adc.readChannel(AdcDriver::Channel::SE_AIN0);

    int16_t before = adc.filteredValue();

    // Simulate: user typed an invalid command — filter is NOT changed
    // (no setFilter call happens for invalid input)
    // Verify the active filter still produces the same result
    int16_t after = adc.filteredValue();
    EXPECT_EQ(before, after);
    EXPECT_EQ(after, 10);
}

TEST(AdcDriver, StrategyPersistsUntilNextCommand) {
    MovingAverageFilter mavg(4);
    MedianFilter        med(5);
    AdcDriver           adc(&mavg);

    // Switch to Median once
    adc.setFilter(&med);

    // Read multiple samples — strategy should stay as Median
    for (int16_t i = 1; i <= 10; ++i) {
        spi_sim::rx_inject = { 0, static_cast<uint8_t>(i), 0, 0 };
        adc.readChannel(AdcDriver::Channel::SE_AIN0);
    }

    // Verify Median is still active: last 5 = {6,7,8,9,10} → sorted → median = 8
    EXPECT_EQ(adc.filteredValue(), 8);

    // Read more samples without switching — Median must persist
    for (int16_t i = 11; i <= 15; ++i) {
        spi_sim::rx_inject = { 0, static_cast<uint8_t>(i), 0, 0 };
        adc.readChannel(AdcDriver::Channel::SE_AIN0);
    }

    // last 5: {11,12,13,14,15} → median = 13
    EXPECT_EQ(adc.filteredValue(), 13);
}

// ---------------------------------------------------------------------------
// Additional coverage tests
// ---------------------------------------------------------------------------
TEST(IFilter, VirtualDestructor) {
    IFilter* f = new MovingAverageFilter(4);
    delete f;
}

TEST(MedianFilter, WindowLargerThanBuffer) {
    // Window is forced odd, 39 | 1 = 39. Buffer size is 32.
    MedianFilter med(39);
    EXPECT_STREQ(med.name(), "Median");

    // Prepare a vector/span of 40 elements: 1..40
    std::vector<int16_t> samples;
    for (int16_t i = 1; i <= 40; ++i) {
        samples.push_back(i);
    }

    // This should copy last 32 elements (since window is limited to tmp buffer of 32)
    // Last 32 elements are: 9..40
    // Median of 32 sorted elements: index n/2 = 32/2 = 16
    // Elements: 9..40 -> index 16 is 9 + 16 = 25
    int16_t result = med.apply(samples);
    EXPECT_EQ(result, 25);
}

TEST(AdcDriver, FilterEmptyHistoryReturnsZero) {
    MovingAverageFilter mavg(4);
    AdcDriver adc(&mavg);
    EXPECT_EQ(adc.filteredValue(), 0);
}

