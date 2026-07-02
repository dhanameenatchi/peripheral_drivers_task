#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_debug, LOG_LEVEL_INF);

#define I2C_NODE DT_NODELABEL(i2c1)

const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

constexpr uint8_t PAV_ADDR = 0x28;

int main(void)
{
    if (!device_is_ready(i2c_dev))
    {
        LOG_ERR("I2C1 not ready");
        return 0;
    }

    LOG_INF("=================================");
    LOG_INF("PAV3015 Register Dump");
    LOG_INF("=================================");

    while (true)
    {
        for (uint8_t reg = 0; reg <= 0x0F; reg++)
        {
            uint8_t value = 0;

            int ret = i2c_reg_read_byte(
                i2c_dev,
                PAV_ADDR,
                reg,
                &value);

            if (ret == 0)
            {
                LOG_INF("Reg 0x%02X = 0x%02X", reg, value);
            }
            else
            {
                LOG_ERR("Reg 0x%02X read failed (%d)", reg, ret);
            }

            k_msleep(20);
        }

        LOG_INF("---------------------------------");

        k_sleep(K_SECONDS(3));
    }

    return 0;
}
