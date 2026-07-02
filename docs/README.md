# HCE Peripheral Drivers

**STM32 NUCLEO-F446RE · Zephyr RTOS · C++20**

Five peripheral driver modules implementing Observer, Factory Method, and Strategy design patterns with 100% test coverage target.

| Module | Pattern | Interface | Key Components |
|--------|---------|-----------|----------------|
| GPIO | Observer | PC13 button, PA5/PA6 LEDs | `ButtonEventBus`, `LedToggleListener`, `UartLogListener` |
| UART | Template | USART2 DMA @ 921600 baud | `RingBuffer<T,N>`, `DmaUart`, `LogPacket` |
| I2C | Factory Method | I2C1 (PB8/PB9) @ 400 kHz | `SensorFactory`, BME280, SHTC3, LPS22HB, PAV3015 |
| SPI | Strategy | SPI1 (PA5-PA7, PB6 CS) | `AdcDriver` (ADS1118), `MovingAverageFilter`, `MedianFilter` |
| CRC | Strategy | Software-only | `FrameCodec`, `Crc16Ccitt`, `Crc8Maxim` |

---

## Quick Start — Run Tests on PC (No Hardware)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)
./build/gpio_test && ./build/uart_test && ./build/i2c_test \
  && ./build/spi_test && ./build/crc_test
```

## Quick Start — Flash to NUCLEO-F446RE

```bash
cd zephyr_app
west build -b nucleo_f446re -- \
  -DCONF_FILE=config/prj.conf \
  -DDTC_OVERLAY_FILE=boards/nucleo_f446re.overlay
west flash
# Open serial: screen /dev/ttyACM0 921600
```

> **Note:** The current Zephyr build activates only the UART DMA module. GPIO, I2C, SPI, and CRC app code exists but is commented out in `main.cpp`. All modules are fully tested on host via GTest.

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
- **[Task Requirements](task_requirements.md)** — Original requirements specification
