// =============================================================================
// i2c_app.cpp — Factory Method pattern on real I2C bus
// Sensors: PAV3015 (0x28), BME280 (0x76), SHTC3 (0x70), LPS22HB (0x5C)
// All share I2C1: SCL=PB8 (D15), SDA=PB9 (D14) @ 400 kHz
// Sampling: PAV3015 @ 50 Hz, LPS22HB @ 25 Hz, BME280+SHTC3 @ 10 Hz
// =============================================================================
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#include <cstdio>
#include <memory>
#include <cmath>

#include "sensor_factory.hpp"

LOG_MODULE_REGISTER(i2c_app, LOG_LEVEL_INF);

// ── Sensor instances — all created via SensorFactory ─────────────────────────
static std::unique_ptr<ISensor> g_bme280;
static std::unique_ptr<ISensor> g_shtc3;
static std::unique_ptr<ISensor> g_lps22hb;
static std::unique_ptr<ISensor> g_pav3015; // PAV3015 replaces FS3000

// ── Typed accessors (downcast is safe: we know what the factory created) ──────
static inline BME280Driver *bme() { return static_cast<BME280Driver *>(g_bme280.get()); }
static inline SHTC3Driver *shtc() { return static_cast<SHTC3Driver *>(g_shtc3.get()); }
static inline LPS22HbDriver *lps() { return static_cast<LPS22HbDriver *>(g_lps22hb.get()); }
static inline PAV3015Driver *pav() { return static_cast<PAV3015Driver *>(g_pav3015.get()); }

// =============================================================================
// i2c_app_init — create all sensor instances and apply initial config
// =============================================================================
void i2c_app_init()
{
    LOG_INF("I2C Bus Initialized");
    LOG_INF("SCL : PB8 (D15)");
    LOG_INF("SDA : PB9 (D14)");
    LOG_INF("Speed : 400 kHz");

    g_bme280 = SensorFactory::create(SensorKind::BME280, 0x76);
    g_shtc3 = SensorFactory::create(SensorKind::SHTC3, 0x70);
    g_lps22hb = SensorFactory::create(SensorKind::LPS22HB, 0x5D);
    g_pav3015 = SensorFactory::create(SensorKind::PAV3015, 0x28);

    if (!g_bme280 || !g_shtc3 || !g_lps22hb || !g_pav3015)
    {
        LOG_ERR("Failed to create one or more sensor drivers");
        return;
    }

    lps()->configODR(3);
    pav()->setRange(PAV3015Driver::RANGE_15MPS);

    LOG_INF("All sensor drivers initialized successfully.");
}

// =============================================================================
// i2c_app_sample — called at 50 Hz from the i2c thread in main.cpp
// CSV format per sensor type:
//   PAV,<ts_ms>,<raw_code>,<vel_mps>
//   BME,<ts_ms>,<temp_C>,<press_hPa>,<hum_pct>
//   LPS,<ts_ms>,<press_hPa_filtered>,<temp_C>
//   SHT,<ts_ms>,<temp_C>,<hum_pct>
// =============================================================================
// =============================================================================
// ── PAV3015: CPAP respiratory air velocity (Called at 50 Hz) ────────────────
// =============================================================================
void i2c_app_sample_pav()
{
    char csv[128];
    uint32_t ts = k_uptime_get_32();

    int32_t raw = pav()->readRaw();
    float velocity = pav()->toSI();

    LOG_INF("====================================================");
    LOG_INF("PAV3015");
    LOG_INF("Timestamp : %u ms", ts);
    LOG_INF("Raw Code  : %d", (int)raw);
    LOG_INF("Velocity  : %.3f m/s", (double)velocity);

    snprintf(csv, sizeof(csv), "PAV,%u,%d,%.3f", ts, (int)raw, (double)velocity);
    LOG_INF("CSV : %s", csv);
}

// =============================================================================
// ── LPS22HB: CPAP airway pressure (Called at 25 Hz) ─────────────────────────
// =============================================================================
void i2c_app_sample_lps()
{
    char csv[128];
    uint32_t ts = k_uptime_get_32();

    float pressure = lps()->readPressureFiltered_hPa();
    float temp = lps()->readTemp_C();

    LOG_INF("----------------------------------------");
    LOG_INF("LPS22HB");
    LOG_INF("Timestamp : %u ms", ts);
    LOG_INF("Pressure  : %.2f hPa", (double)pressure);
    LOG_INF("Temp      : %.2f C", (double)temp);

    snprintf(csv, sizeof(csv), "LPS,%u,%.2f,%.2f", ts, (double)pressure, (double)temp);
    LOG_INF("CSV : %s", csv);
}

// =============================================================================
// ── Ambient Sensors: BME280 + SHTC3 (Called at 10 Hz) ────────────────────────
// =============================================================================
void i2c_app_sample_ambient()
{
    char csv[128];
    uint32_t ts = k_uptime_get_32();

    // BME280 Sampling
    {
        float temp = bme()->readTemperature_C();
        float pressure = bme()->readPressure_hPa();
        float humidity = bme()->readHumidity_pct();

        LOG_INF("----------------------------------------");
        LOG_INF("BME280");
        LOG_INF("Timestamp : %u ms", ts);
        LOG_INF("Temperature : %.2f C", (double)temp);
        LOG_INF("Pressure    : %.2f hPa", (double)pressure);
        LOG_INF("Humidity    : %.1f %%", (double)humidity);

        snprintf(csv, sizeof(csv), "BME,%u,%.2f,%.2f,%.1f", ts, (double)temp, (double)pressure, (double)humidity);
        LOG_INF("CSV : %s", csv);
    }

    // SHTC3 Sampling
    {
        float temp = shtc()->readTemperature_C();
        float humidity = shtc()->readHumidity_pct();

        if (std::isnan(temp) || std::isnan(humidity))
        {
            LOG_ERR("SHTC3 measurement failed");
        }
        else
        {
            LOG_INF("----------------------------------------");
            LOG_INF("SHTC3");
            LOG_INF("Timestamp   : %u ms", ts);
            LOG_INF("Temperature : %.2f C", (double)temp);
            LOG_INF("Humidity    : %.1f %%RH", (double)humidity);

            snprintf(csv, sizeof(csv), "SHT,%u,%.2f,%.1f", ts, (double)temp, (double)humidity);
            LOG_INF("CSV : %s", csv);
        }
    }
    LOG_INF("====================================================");
}