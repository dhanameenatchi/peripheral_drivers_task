#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hce_main, LOG_LEVEL_INF);

// ── Forward declarations of module entry points ──────────────────────────────
#if MODULE_GPIO
void gpio_app_init(void);
#endif

#if MODULE_UART
#ifdef CONFIG_UART_ASYNC_API
void uart_dma_init(void);
void uart_dma_demo_loop(void);
#endif
#endif

#if MODULE_I2C
void i2c_app_init(void);
void i2c_app_sample_pav(void);
void i2c_app_sample_lps(void);
void i2c_app_sample_ambient(void);
#endif

#if MODULE_SPI
void spi_app_init(void);
void spi_app_sample(void);
#endif

#if MODULE_CRC
void crc_app_init(void);
void crc_app_demo(void);
void crc_loopback_service(void);
bool crc_is_busy(void);
#endif

int main(void) {
    LOG_INF("====================================================");
    LOG_INF("HCE Integrated Peripheral Application Starting");
    
    // Log active modules/modes
#if MODULE_GPIO && MODULE_UART && MODULE_I2C && MODULE_SPI && MODULE_CRC
    LOG_INF("Active Mode: ALL MODULES (Cooperative Loop)");
#else
    #if MODULE_GPIO
        LOG_INF("Active Module: GPIO");
    #endif
    #if MODULE_UART
        LOG_INF("Active Module: UART");
    #endif
    #if MODULE_I2C
        LOG_INF("Active Module: I2C");
    #endif
    #if MODULE_SPI
        LOG_INF("Active Module: SPI");
    #endif
    #if MODULE_CRC
        LOG_INF("Active Module: CRC");
    #endif
#endif
    LOG_INF("====================================================");

    // ── Module Initializations ────────────────────────────────────────────────
#if MODULE_GPIO
    gpio_app_init();
#endif
    
#if MODULE_UART
#ifdef CONFIG_UART_ASYNC_API
    uart_dma_init();
#else
    LOG_INF("UART DMA App skipped (requires CONFIG_UART_ASYNC_API)");
#endif
#endif
    
#if MODULE_I2C
    i2c_app_init();
#endif
    
#if MODULE_SPI
    spi_app_init();
#endif
    
#if MODULE_CRC
    crc_app_init();
#endif

    // ── Scheduling state (uptime tracking) ───────────────────────────────────
#if MODULE_I2C || MODULE_SPI || MODULE_CRC
    uint32_t last_50hz_ticks = 0;   // 20 ms interval (PAV3015)
    uint32_t last_25hz_ticks = 0;   // 40 ms interval (LPS22HB)
    uint32_t last_10hz_ticks = 0;   // 100 ms interval (BME280 + SHTC3, SPI ADS1118)
    uint32_t last_0_5hz_ticks = 0;  // 2000 ms interval (CRC demo)
    constexpr uint32_t SPI_SAMPLE_PERIOD_MS = (100*8);

    LOG_INF("Cooperative scheduler started.");

    while (true) {
        uint32_t now = k_uptime_get_32();

#if MODULE_I2C
        // 50 Hz Task: PAV3015 Flow Sensor (20 ms)
        if (now - last_50hz_ticks >= (20 *8)) {
            last_50hz_ticks = now;
            i2c_app_sample_pav();
        }

        // 25 Hz Task: LPS22HB Airway Pressure (40 ms)
        if (now - last_25hz_ticks >= (40 * 8)) {
            last_25hz_ticks = now;
            i2c_app_sample_lps();
        }
#endif

        // 10 Hz Tasks: BME280 + SHTC3 Ambient & SPI ADS1118 ADC (100 ms)
        if (now - last_10hz_ticks >= SPI_SAMPLE_PERIOD_MS) {
            last_10hz_ticks = now;
#if MODULE_I2C
            i2c_app_sample_ambient();
#endif
#if MODULE_SPI
            spi_app_sample();
#endif
        }

#if MODULE_CRC
        // 0.5 Hz Task: CRC Frame Codec Demo (2000 ms)
        if (now - last_0_5hz_ticks >= 2000) {
            last_0_5hz_ticks = now;
            // crc_app_demo();
        }

        // Continuous Loopback Service (Non-blocking)
        crc_loopback_service();
#endif

        // Cooperative yield — only sleep if not in the middle of receiving a CRC frame
#if MODULE_CRC
        if (!crc_is_busy()) {
            k_msleep(1);
        }
#else
        k_msleep(1);
#endif
    }
#else
    // If only GPIO/UART are built, run the UART DMA demo loop
#if MODULE_UART && defined(CONFIG_UART_ASYNC_API)
    uart_dma_demo_loop();  // never returns — streams packets + LED heartbeat
#else
    while (true) {
        k_msleep(1000);
    }
#endif
#endif

    return 0;
}