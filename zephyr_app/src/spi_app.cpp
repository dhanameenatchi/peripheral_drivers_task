// =============================================================================
// spi_app.cpp — Strategy pattern: ADS1118 ADC + swappable filter
// SPI1: SCK=PA5(D13), MISO=PA6(D12), MOSI=PA7(D11), CS=PB6(D10)
// Runtime filter swap via UART command: "filter mavg" / "filter median"
// =============================================================================
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <cstring>
#include "adc_driver.hpp"

LOG_MODULE_REGISTER(spi_app, LOG_LEVEL_INF);

static MovingAverageFilter g_mavg(8);
static MedianFilter        g_median(5);
static AdcDriver           g_adc(&g_mavg);   // starts with MovingAverage

// ── UART command buffer ──────────────────────────────────────────────────────
static constexpr size_t CMD_BUF_SIZE = 64;
static char              g_cmd_buf[CMD_BUF_SIZE];
static size_t            g_cmd_idx = 0;
static const struct device* g_uart_dev = nullptr;

// ── Active strategy tracking ─────────────────────────────────────────────────
static const char* g_active_name = "MovingAverage";

// ---------------------------------------------------------------------------
// spi_process_command — parse and execute a UART command line
// ---------------------------------------------------------------------------
static void spi_process_command(const char* cmd) {
    // Strip leading whitespace
    while (*cmd == ' ' || *cmd == '\t') ++cmd;

    if (std::strncmp(cmd, "filter mavg", 11) == 0) {
        g_adc.setFilter(&g_mavg);
        g_active_name = g_mavg.name();
        LOG_INF("Filter changed to MovingAverage");
    } else if (std::strncmp(cmd, "filter median", 13) == 0) {
        g_adc.setFilter(&g_median);
        g_active_name = g_median.name();
        LOG_INF("Filter changed to Median");
    } else if (cmd[0] != '\0') {
        LOG_WRN("Unknown command.");
        LOG_WRN("Valid commands:");
        LOG_WRN("  filter mavg");
        LOG_WRN("  filter median");
    }
}

// ---------------------------------------------------------------------------
// UART IRQ callback — accumulates characters into line buffer
// ---------------------------------------------------------------------------
static void uart_irq_handler(const struct device* dev, void* /* user_data */) {
    uart_irq_update(dev);
    if (!uart_irq_rx_ready(dev)) {
        return;
    }

    uint8_t c;
    while (uart_fifo_read(dev, &c, 1) == 1) {
        if (c == '\n' || c == '\r') {
            if (g_cmd_idx > 0) {
                g_cmd_buf[g_cmd_idx] = '\0';
                spi_process_command(g_cmd_buf);
                g_cmd_idx = 0;
            }
        } else if (g_cmd_idx < CMD_BUF_SIZE - 1) {
            g_cmd_buf[g_cmd_idx++] = static_cast<char>(c);
        }
    }
}

// =============================================================================
// spi_app_init — initialise SPI ADC and UART command listener
// =============================================================================
void spi_app_init() {
    LOG_INF("====================================================");
    LOG_INF("SPI ADS1118 ADC Module Initialized");
    // LOG_INF("SCK  : PA5 (D13)");
    // LOG_INF("MISO : PA6 (D12)");
    // LOG_INF("MOSI : PA7 (D11)");
    // LOG_INF("CS   : PB6 (D10)");
    // LOG_INF("Mode : SPI Mode 1 (CPOL=0, CPHA=1)");
    // LOG_INF("Word : 16-bit");
    // LOG_INF("Speed: 1 MHz");
    // LOG_INF("Default filter: MovingAverage(8)");
    LOG_INF("Commands: 'filter mavg' / 'filter median'");
    LOG_INF("====================================================");

    // Set up UART interrupt-driven RX for command input
    g_uart_dev = DEVICE_DT_GET(DT_NODELABEL(usart2));
    if (device_is_ready(g_uart_dev)) {
        uart_irq_callback_set(g_uart_dev, uart_irq_handler);
        uart_irq_rx_enable(g_uart_dev);
        LOG_INF("UART command listener ready on USART2");
    } else {
        LOG_ERR("USART2 not ready — runtime commands disabled");
    }
}

// =============================================================================
// spi_app_sample — called at 10 Hz from main loop
// =============================================================================
void spi_app_sample() {
    uint32_t ts = k_uptime_get_32();

    // Read raw ADC from AIN0 single-ended
int16_t raw =
    g_adc.readChannel(
    AdcDriver::Channel::SE_AIN0,      // Target Input (AIN0 vs GND)
    AdcDriver::PGA::FSR_2_048V,       // Real ±4.096V gain mask (0x0200)
    AdcDriver::Mode::CONTINUOUS       // Continuous mode to bypass driver bug
);

    // Compute both filter outputs directly on shared history
    // (no strategy swapping — active filter stays untouched)
    std::span<const int16_t> hist(g_adc.history());
    int16_t ma_val  = g_mavg.apply(hist);
    int16_t med_val = g_median.apply(hist);

    // Formatted UART output
    LOG_INF("----------------------------------------------------");
    LOG_INF("Timestamp : %u ms", ts);
    LOG_INF("Channel   : SE_AIN0");
    LOG_INF("Raw ADC   : %d", (int)raw);
    LOG_INF("MovAvg    : %d", (int)ma_val);
    LOG_INF("Median    : %d", (int)med_val);
    LOG_INF("Strategy  : %s", g_active_name);
}
