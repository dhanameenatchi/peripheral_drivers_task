#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hce_main, LOG_LEVEL_INF);

void i2c_app_init();
void i2c_app_sample_pav();   // Splitting out concerns
void i2c_app_sample_lps();
void i2c_app_sample_ambient(); // BME + SHT

int main(void) {
    LOG_INF("HCE I2C Sensor Suite starting on NUCLEO-F446RE");
    
    i2c_app_init();
    
    uint32_t tick_count = 0;

    while (true) {
        // Base loop rate: 50 Hz (every 20 ms)
        
        // 50 Hz Task
        i2c_app_sample_pav();

        // 25 Hz Task (Every 2 ticks)
        if (tick_count % 2 == 0) {
            i2c_app_sample_lps();
        }

        // 10 Hz Task (Every 5 ticks)
        if (tick_count % 5 == 0) {
            i2c_app_sample_ambient();
        }

        tick_count++;
        k_msleep(20); // Accurate periodic window
    }
    return 0;
}