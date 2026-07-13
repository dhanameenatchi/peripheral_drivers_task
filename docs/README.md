# HCE Peripheral Drivers

**STM32 NUCLEO-F446RE · Zephyr RTOS · C++20**

Five peripheral driver modules implementing Observer, Factory Method, and Strategy design patterns with 100% test coverage target.

| Module | Pattern | Interface | Key Components |
|--------|---------|-----------|----------------|
| GPIO | Observer | PC13 button, PB0/PB1 LEDs | `ButtonEventBus`, `LedToggleListener` (×2), `UartLogListener` |
| UART | Template | USART2 DMA @ 921600 baud | `RingBuffer<T,N>`, `DmaUart`, `LogPacket` |
| I2C | Factory Method | I2C1 (PB8/PB9) @ 400 kHz | `SensorFactory`, BME280, SHTC3, LPS22HB, PAV3015 |
| SPI | Strategy | SPI1 (PA5-PA7, PB6 CS) | `AdcDriver` (ADS1118), `MovingAverageFilter`, `MedianFilter` |
| CRC | Strategy | Software-only | `FrameCodec`, `Crc16Ccitt`, `Crc8Maxim` |

---

## Quick Start — Run Tests on PC (No Hardware)

```bash
#For all modules run
cmake -B build -DCMAKE_BUILD_TYPE=Debug 
make -C build -j$(nproc)
./build/gpio_test && ./build/uart_test && ./build/i2c_test \
  && ./build/spi_test && ./build/crc_test
```
```bash

#Host side Testing

cd ~/zephyrproject/zephyr_hce_task/hce_drivers

rm -rf host_build
cmake -B host_build -DCMAKE_BUILD_TYPE=Debug

cmake --build host_build --target gpio_test
./host_build/gpio_test
cmake --build host_build --target gpio_coverage
xdg-open host_build/gpio_coverage/index.html

cmake --build host_build --target uart_test
./host_build/uart_test
cmake --build host_build --target uart_bench
./host_build/uart_bench

cmake --build host_build --target i2c_test
./host_build/i2c_test

cmake --build host_build --target spi_test
./host_build/spi_test

cmake --build host_build --target crc_test
./host_build/crc_test

#Hardware Testing
rm -rf build_gpio
west build -b nucleo_f446re zephyr_app -d build_gpio -- -DCONF_FILE=config/prj_gpio.conf
west flash --build-dir build_gpio
minicom -D /dev/ttyACM0 -b 921600

rm -rf build_uart
west build -b nucleo_f446re zephyr_app -d build_uart -- -DCONF_FILE=config/prj_uart.conf
west flash --build-dir build_uart
minicom -D /dev/ttyACM0 -b 921600

rm -rf build_i2c
west build -b nucleo_f446re zephyr_app -d build_i2c -- -DCONF_FILE=config/prj_i2c.conf
west flash --build-dir build_i2c
minicom -D /dev/ttyACM0 -b 921600

rm -rf build_spi
west build -b nucleo_f446re zephyr_app -d build_spi -- -DCONF_FILE=config/prj_spi.conf
west flash --build-dir build_spi
minicom -D /dev/ttyACM0 -b 921600
filter mavg
filter median

rm -rf build_crc16
west build -b nucleo_f446re zephyr_app -d build_crc16 --pristine -- -DCONF_FILE="config/prj.conf;config/prj_crc_loopback.conf"

west flash --build-dir build_crc16

# Wait a few seconds for boot, then test
python3 scripts/loopback_test.py --port /dev/ttyACM0 --baud 921600 --crc 16


```
## Quick Start — Flash to NUCLEO-F446RE

```bash
cd zephyr_app
# Build with all modules active in a cooperative scheduler loop:
west build -b nucleo_f446re -- -DDEMO_MODE=ALL
# Or compile only one selected module, e.g., GPIO_ONLY:
west build -b nucleo_f446re -- -DDEMO_MODE=GPIO_ONLY

west flash
# Open serial: screen /dev/ttyACM0 921600
```

> **Note:** The integrated Zephyr build executes a non-blocking cooperative scheduling loop running all enabled module tasks at their required rates (up to 50 Hz). You can select a single module at build time via the `-DDEMO_MODE=...` flag.

## Generate Coverage Report

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make -C build -j$(nproc)
make -C build coverage
# Open: build/coverage/index.html
```

## CRC Python Loopback Test

```bash
python3 scripts/loopback_test.py --mock
# 12/12 tests passed
```

## I2C Sensor Addresses

| Sensor | Address | Function |
|--------|---------|----------|
| PAV3015 | 0x28 | CPAP air velocity (0–15 m/s) |
| LPS22HB | 0x5C | CPAP airway pressure |
| SHTC3 | 0x70 | Temperature / Humidity |
| BME280 | 0x76 | Temperature / Pressure / Humidity |

## Documentation

- **[Hardware Guide](HARDWARE_GUIDE.md)** — Pin mapping, wiring diagrams, power requirements, bring-up, troubleshooting
- **[Running & Testing Guide](RUNNING_AND_TESTING_GUIDE.md)** — Prerequisites, build, flash, test, coverage, module-by-module verification
- **[Test Checklist](TEST_CHECKLIST.md)** — Requirements mapping and verification check status
- **[Task Requirements](task_requirements.md)** — Original requirements specification
