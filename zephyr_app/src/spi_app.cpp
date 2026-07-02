// =============================================================================
// spi_app.cpp — Strategy pattern: ADS1118 ADC + swappable filter
// SPI1: SCK=PA5(D13), MISO=PA6(D12), MOSI=PA7(D11), CS=PB6(D10)
// =============================================================================
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "adc_driver.hpp"

LOG_MODULE_REGISTER(spi_app, LOG_LEVEL_INF);

static MovingAverageFilter g_mavg(8);
static MedianFilter        g_median(5);
static AdcDriver           g_adc(&g_mavg);   // starts with MovingAverage

void spi_app_init() {
    LOG_INF("SPI init: SCK=PA5 MISO=PA6 MOSI=PA7 CS=PB6");
    LOG_INF("ADS1118 ADC ready. Default filter: MovingAverage(8)");
}

// Called at 100 Hz from spi thread in main.cpp
void spi_app_sample() {
    char csv[96];
    uint32_t ts = k_uptime_get_32();

    int16_t raw      = g_adc.readChannel(AdcDriver::Channel::SE_AIN0);
    int16_t filtered = g_adc.filteredValue();

    snprintf(csv, sizeof(csv), "ADC,%u,%d,%d\r\n", ts, raw, filtered);
    LOG_INF("%s", csv);
}

// Call this from UART shell command: "filter mavg" or "filter median"
void spi_set_filter(const char* name) {
    if (name[0] == 'm' && name[1] == 'a') {
        g_adc.setFilter(&g_mavg);
        LOG_INF("Filter swapped to MovingAverage");
    } else {
        g_adc.setFilter(&g_median);
        LOG_INF("Filter swapped to Median");
    }
}
