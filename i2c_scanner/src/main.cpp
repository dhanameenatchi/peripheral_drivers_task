#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_scanner, LOG_LEVEL_INF);

int main()
{
    const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c1));

    if (!device_is_ready(i2c))
    {
        LOG_ERR("I2C1 not ready");
        return 0;
    }

    LOG_INF("Scanning I2C bus...");

    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
        uint8_t dummy;

        if (i2c_read(i2c, &dummy, 1, addr) == 0)
        {
            LOG_INF("Found device at 0x%02X", addr);
        }
    }

    LOG_INF("Scan complete.");

    while (1)
    {
        k_sleep(K_SECONDS(5));
    }
}