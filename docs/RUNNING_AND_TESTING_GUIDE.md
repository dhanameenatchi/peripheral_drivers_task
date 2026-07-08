# HCE Peripheral Drivers — Running & Testing Guide

## Build, Flash, Test, and Validate

---

## 1. Project Prerequisites

### Operating System

Linux (Ubuntu 22.04+ recommended), macOS, or Windows with WSL2.

### Required Software

| Tool | Version | Purpose |
|------|---------|---------|
| Python | 3.8+ | `west` meta-tool, loopback test script |
| west | latest | Zephyr workspace manager |
| Zephyr SDK | 0.16.x | ARM cross-compiler toolchain |
| CMake | 3.20+ | Build system |
| Ninja | latest | Build backend (used by west) |
| GCC (host) | 12+ | C++20 support for host GTest builds |
| GTest | latest | Google Test framework (host tests) |
| lcov / gcov | latest | Code coverage |
| screen or minicom | latest | Serial terminal |

### Install Dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y cmake ninja-build gcc g++ python3 python3-pip \
    libgtest-dev lcov screen minicom openocd stlink-tools

# Build GTest from source if needed
cd /usr/src/gtest && sudo cmake . && sudo make && sudo make install
```

---

## 2. Repository Setup

### Clone the Repository

```bash
# If not already in a Zephyr workspace:
git clone <repo-url> ~/zephyrproject/hce_drivers
```

### Zephyr SDK Setup

```bash
# Install west
pip install west

# Initialize Zephyr workspace (if starting fresh)
west init ~/zephyrproject
cd ~/zephyrproject
west update
west zephyr-export
pip install -r zephyr/scripts/requirements.txt

# Download and install Zephyr SDK
# From: https://github.com/zephyrproject-rtos/sdk-ng/releases
# Download: zephyr-sdk-0.16.x-linux-x86_64.tar.xz
cd ~
tar xf zephyr-sdk-0.16.x-linux-x86_64.tar.xz
cd ~/zephyr-sdk-0.16.x && ./setup.sh
```

### Environment Variables

```bash
export ZEPHYR_BASE=~/zephyrproject/zephyr
export ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk-0.16.x
source ~/zephyrproject/zephyr/zephyr-env.sh
```

Add to `~/.bashrc` for persistence.

### Project Structure

```
hce_drivers/
├── CMakeLists.txt                  ← Host GTest build (PC-only)
├── gpio/
│   ├── include/   digital_io.hpp, listeners.hpp
│   ├── src/       digital_io.cpp, listeners.cpp
│   └── test/      gpio_test.cpp
├── uart/
│   ├── include/   ring_buffer.hpp, dma_uart.hpp
│   ├── src/       dma_uart.cpp
│   ├── test/      uart_test.cpp
│   └── bench/     bench_uart.cpp
├── i2c/
│   ├── include/   sensor.hpp, sensor_factory.hpp, bme280_driver.hpp,
│   │              shtc3_driver.hpp, lps22hb_driver.hpp, pav3015_driver.hpp
│   ├── src/       sensor_factory.cpp, bme280_driver.cpp, shtc3_driver.cpp,
│   │              lps22hb_driver.cpp, pav3015_driver.cpp
│   └── test/      test_concepts.cpp, test_sensor_factory.cpp, test_bme280.cpp,
│                  test_shtc3.cpp, test_lps22hb.cpp, test_fs3000.cpp,
│                  test_error_injection.cpp, test_csv_output.cpp
├── spi/
│   ├── include/   adc_driver.hpp, filter.hpp, moving_average_filter.hpp,
│   │              median_filter.hpp
│   ├── src/       adc_driver.cpp, moving_average_filter.cpp, median_filter.cpp
│   └── test/      spi_test.cpp
├── crc/
│   ├── include/   crc_strategy.hpp, crc16_ccitt.hpp, crc8_maxim.hpp,
│   │              frame_codec.hpp
│   ├── src/       crc16_ccitt.cpp, crc8_maxim.cpp, frame_codec.cpp
│   └── test/      crc_test.cpp
├── zephyr_stubs/
│   └── include/   zephyr_gpio_mock.hpp, zephyr_uart_mock.hpp,
│                  zephyr_i2c_mock.hpp, zephyr_spi_mock.hpp,
│                  zephyr_crc_mock.hpp, zephyr/drivers/i2c.h,
│                  zephyr/kernel.h, zephyr/logging/log.h
├── zephyr_app/
│   ├── CMakeLists.txt              ← Zephyr on-target build
│   ├── config/prj.conf             ← Kconfig options
│   ├── boards/nucleo_f446re.overlay ← Pin assignments
│   └── src/       main.cpp, gpio_app.cpp, uart_app.cpp,
│                  i2c_app.cpp, spi_app.cpp, crc_app.cpp
├── scripts/
│   └── loopback_test.py            ← CRC Python loopback test
└── docs/
    ├── HARDWARE_GUIDE.md
    ├── README.md
    └── task_requirements.md
```

---

## 3. Build

### Host GTest Build (No Hardware Needed)

```bash
cd ~/zephyrproject/zephyr_hce_task/hce_drivers
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)
```

This produces five test executables and a benchmark:

| Executable | Module |
|-----------|--------|
| `build/gpio_test` | GPIO Observer Pattern |
| `build/uart_test` | UART RingBuffer + DMA |
| `build/i2c_test` | I2C Factory Method + all sensors |
| `build/spi_test` | SPI ADC + Strategy filters |
| `build/crc_test` | CRC Frame Protocol |
| `build/uart_bench` | UART throughput benchmark |

### Zephyr On-Target Build

```bash
cd ~/zephyrproject/hce_drivers/zephyr_app

west build -b nucleo_f446re \
  -- \
  -DCONF_FILE=config/prj.conf \
  -DDTC_OVERLAY_FILE=boards/nucleo_f446re.overlay
```

Expected build output:
```
-- Board: nucleo_f446re
[xxx/xxx] Linking CXX executable zephyr/zephyr.elf
Memory region   Used Size  Region Size  %age Used
          FLASH:     xxxxx B       512 KB     xx.xx%
           SRAM:     xxxxx B       128 KB     xx.xx%
```

> **Note:** The current Zephyr build (`main.cpp`) only activates the UART DMA module. GPIO, I2C, SPI, and CRC app initialization calls are commented out. To enable all modules, uncomment the full `main.cpp` and update `zephyr_app/CMakeLists.txt` to include all source files and include paths. The commented-out full build configuration is preserved at the top of both files.

### Clean Build

```bash
# Host
rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Debug && make -C build -j$(nproc)

# Zephyr (Build all modules together)
cd zephyr_app && rm -rf build && west build -b nucleo_f446re -- -DCONF_FILE=config/prj.conf -DDTC_OVERLAY_FILE=boards/nucleo_f446re.overlay -DDEMO_MODE=ALL

# Zephyr (Compile only one selected module, e.g., GPIO_ONLY, UART_ONLY, I2C_ONLY, SPI_ONLY, CRC_ONLY)
west build -b nucleo_f446re -- -DDEMO_MODE=GPIO_ONLY
```

---

## 4. Flash

### Standard Flash

```bash
cd ~/zephyrproject/zephyr_hce_task/hce_drivers/zephyr_app

#spi only build
west build -b nucleo_f446re -d build --pristine -- -DMODULE=SPI -DCONF_FILE="config/prj.conf;config/prj_spi.conf"
west flash -d build
minicom -D /dev/ttyACM0 -b 921600
filter mavg
filter median

west flash
```

Expected:
```
-- west flash: using runner openocd
-- OpenOCD: flashing file: build/zephyr/zephyr.bin
Programmed xxxxx bytes at 0x8000000
```

### Recovery / Erase

```bash
# Full chip erase
west flash --erase

# If ST-Link is unresponsive, hold RESET button while running:
west flash --runner openocd
```

### Debug Build

```bash
west build -b nucleo_f446re -- -DCONF_FILE=config/prj.conf -DDTC_OVERLAY_FILE=boards/nucleo_f446re.overlay -DCMAKE_BUILD_TYPE=Debug
west debug   # Opens GDB session
```

---

## 5. UART Serial Monitor

### Connection

USART2 (PA2 TX, PA3 RX) is routed through the ST-Link Virtual COM Port. No external wiring needed — just the USB cable.

### Settings

| Parameter | Value |
|-----------|-------|
| Baud rate | 921600 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| Line ending | CR+LF (`\r\n`) |

### Open Terminal

```bash
# Linux / macOS
screen /dev/ttyACM0 921600
# Quit: Ctrl+A then K

# Alternative
minicom -D /dev/ttyACM0 -b 921600
# Quit: Ctrl+A then X

# Windows (PuTTY)
# Serial → COM port (check Device Manager) → 921600 → Open
```

### Expected Boot Log (Current UART-Only Build)

```
*** Booting Zephyr OS build v4.x.x ***
[00:00:00.001] <inf> hce_main: HCE GPIO-Only Build starting on NUCLEO-F446RE
[00:00:00.001] <inf> hce_main: Zephyr version: 4.x.x
[00:00:00.002] <inf> uart_dma: DMA UART ready: TX=PA2 RX=PA3 @ 921600
[00:00:00.002] <inf> hce_main: GPIO observer ready. Press B1 to test.
```

### Expected Sensor CSV Output (When All Modules Enabled)

```
PAV,1020,1458,2.000
LPS,1020,1013.2500,24.80
BME,1020,24.50,1013.25,55.1
SHT,1020,24.30,54.8
ADC,5000,16384,16200
```

---

## 6. Running Individual Modules

### GPIO Module — Observer Pattern

**Purpose:** Button press → 10 ms debounce → notifies `LedToggleListener` (toggles LED) and `UartLogListener` (logs timestamp).

**Required hardware:** NUCLEO-F446RE, optional external yellow LED with 220 Ω resistor on PA6.

**Host test:**
```bash
./build/gpio_test
```

**Expected console output:**
```
[==========] Running 18 tests from 5 test suites.
----- 9 ms -----
Timer NOT expired
----- 10 ms -----
----- 11 ms -----
[==========] 18 tests from 5 test suites ran.
[  PASSED  ] 18 tests.
```

**Tests cover:** Subscribe/unsubscribe/notify, max listener capacity (8), debounce boundary (9/10/11 ms), LED toggle on press only, UART log format `[T=<ms>ms] BUTTON <PRESSED|RELEASED>`, log saturation at MAX_LOG (32), `DigitalInput`/`DigitalOutput` read/write/toggle, `IButtonListener` virtual destructor.

**Hardware behavior (when Zephyr app enables GPIO):** Press B1 → yellow LED toggles, serial output shows timestamped events.

---

### UART Module — RingBuffer + DMA

**Purpose:** Lock-free SPSC `RingBuffer<T,N>` with DMA UART driver for non-blocking structured log transmission at 921600 baud.

**Required hardware:** NUCLEO-F446RE + USB cable (UART via ST-Link VCP).

**Host test:**
```bash
./build/uart_test
```

**Expected console output:**
```
[==========] Running 22 tests from 5 test suites.
[  PASSED  ] 22 tests.
```

**Benchmark:**
```bash
./build/uart_bench
```

Expected output:
```
=== UART Throughput Benchmark — 1000 packets @ 921600 baud ===

CPU-blocking UART            packets=1000  overflows=0  ...  CPU_free=NO
DMA UART (ring enqueue)      packets=1000  overflows=0  ...  CPU_free=YES

DMA enqueue is X.Xx faster per call (CPU free during TX)

--- Overflow injection test ---
  Pushed: 300   Succeeded: 255   Failed(overflow): 45
  txOverflow counter: 45
  Result: PASS
```

**Tests cover:** RingBuffer empty/full/wrap-around, overflow counter saturation, FIFO ordering, multiple wrap cycles, reset, concurrent SPSC (1000 iterations), overflow injection (200 pushes into 7-slot buffer), DmaUart log/drain/rxPush/rxPop, all log level strings (DBG/INF/WRN/ERR), CRLF line endings, partial drain, LogPacket truncation/null, 1 kHz throughput simulation, UART mock stubs.

**Failure cases:** TX overflow when ring is full → `log()` returns `false`, `txOverflow()` increments. RX overflow → `rxPush()` returns `false`.

---

### I2C Module — Factory Method Pattern

**Purpose:** `SensorFactory::create(SensorKind, addr)` produces sensor drivers implementing `ISensor` interface. C++20 `SensorType_c` concept enforces `readRaw()`, `toSI()`, `deviceId()` at compile time.

**Required hardware:** NUCLEO-F446RE + BME280 + SHTC3 + LPS22HB + PAV3015 + 2× 4.7 kΩ pull-ups.

**Host test:**
```bash
./build/i2c_test
```

**Expected console output:**
```
[==========] Running XX tests from 1 test suite.
[  PASSED  ] XX tests.
```

**Tests cover:**
- **SensorFactory:** Creates correct concrete type per `SensorKind` enum, default addresses
- **BME280:** Register reads (REG_TEMP_MSB=0xFA, REG_PRESS_MSB=0xF7, REG_HUM_MSB=0xFD), SI conversions
- **SHTC3:** Register reads (REG_TEMP_H=0x00, REG_HUM_H=0x02, CMD_WAKEUP=0x35), SI conversions
- **LPS22HB:** Register reads (REG_PRESS_XL=0x28, REG_TEMP_L=0x2B), `configODR()` writes to REG_CTRL1=0x10, 4-tap moving average filter, ramp input averaging correctness
- **PAV3015:** Checksum valid/invalid/too-short/zero, interpolation at all knots (15 m/s and 7 m/s tables), midpoint interpolation, min/max raw code clamp, `readVelocity_mps()` at knot values, range selection writes to CTRL register
- **Error injection:** I2C NACK simulation
- **CSV output:** Format validation
- **Concepts:** `static_assert(SensorType_c<BME280Driver>)` etc.

---

### SPI Module — Strategy Pattern

**Purpose:** `AdcDriver` wraps ADS1118 16-bit ADC over SPI Mode 1. `IFilter` interface with `MovingAverageFilter` and `MedianFilter` strategies, swappable at runtime.

**Required hardware:** NUCLEO-F446RE + ADS1118 breakout + potentiometer.

**Host test:**
```bash
./build/spi_test
```

**Expected console output:**
```
[==========] Running 14 tests from 3 test suites.
[  PASSED  ] 14 tests.
```

**Tests cover:**
- **MovingAverageFilter:** All-equal (→ same value), ramp (→ average of last N), impulse (→ 0), empty (→ 0), window > input
- **MedianFilter:** All-equal, ramp, impulse (rejects outlier), odd window forced, empty
- **AdcDriver:** SPI RX data injection, SPI error returns `INT16_MIN`, channel selection TX capture, history accumulates, clear history
- **Strategy swap:** Mid-stream swap preserves history, filter name changes
- **No filter:** Returns last sample; empty history returns 0

**Runtime filter swap (on hardware, when enabled):**
```
uart:~$ filter mavg     # Switch to MovingAverage
uart:~$ filter median   # Switch to Median
```

---

### CRC Module — Strategy Pattern + Frame Protocol

**Purpose:** Binary frame protocol `[SOF 0xAA | Len | Cmd | Payload ≤32B | CRC]` with pluggable CRC strategy. Medical context: IEC 60601-1-8 alarm frame integrity.

**Required hardware:** None (pure software). Python loopback test needs only PC.

**Host test:**
```bash
./build/crc_test
```

**Expected console output:**
```
[==========] Running 17 tests from 4 test suites.
[  PASSED  ] 17 tests.
```

**Python loopback test:**
```bash
python3 scripts/loopback_test.py --mock
```

Expected:
```
Using software mock loopback (no hardware required)

──────────────────────────────────────────────────
  Running with CRC16
──────────────────────────────────────────────────

──────────────────────────────────────────────────
  Running with CRC8
──────────────────────────────────────────────────

══════════════════════════════════════════════════
  TEST RESULTS
══════════════════════════════════════════════════
  [✓] CRC16-CCITT known vector
  [✓] CRC8-Maxim known vector
  [✓] Round-trip          (×2, CRC16 + CRC8)
  [✓] Single-bit error    (×2)
  [✓] Frame drop          (×2)
  [✓] Max payload 32B     (×2)
  [✓] Zero-length payload (×2)

  12/12 tests passed
══════════════════════════════════════════════════
```

**With real hardware:**
```bash
python3 scripts/loopback_test.py --port /dev/ttyACM0 --baud 921600
```

**Tests cover:** CRC16-CCITT known vector ("123456789" → 0x29B1), CRC8-Maxim known vector, encode→decode round-trip, single-bit error detection, zero-length payload, max 32-byte payload, wrong SOF, too-short buffer, null output, buffer-too-small, truncated decode.

---

## 7. Unit Testing

### Test Framework

- **Framework:** Google Test (GTest) with `gtest_main`
- **Build system:** CMake with `gtest_discover_tests()` for CTest integration
- **HAL mocking:** Static global mock objects in `zephyr_stubs/include/` — no heap, no dynamic mocks
- **ISR stubs:** `k_timer`, `gpio_pin_*`, `i2c_write_read_dt`, `spi_transceive` are replaced by simulation functions
- **Standard:** C++20 (`-std=c++20`)

### Run All Tests via CTest

```bash
cd ~/zephyrproject/hce_drivers
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)
cd build && ctest --output-on-failure
```

### Run Individual Test Executables

```bash
./build/gpio_test       # 18 tests
./build/uart_test       # 22 tests
./build/i2c_test        # ~40+ tests
./build/spi_test        # 14 tests
./build/crc_test        # 17 tests
```

### Run All Tests in One Command

```bash
./build/gpio_test && ./build/uart_test && ./build/i2c_test \
  && ./build/spi_test && ./build/crc_test
```

### Test Architecture

| Layer | PC GTest Build | Zephyr Hardware Build |
|-------|---------------|----------------------|
| HAL include | `zephyr_gpio_mock.hpp` etc. | `<zephyr/drivers/gpio.h>` etc. |
| Switch | `#else` branch | `#ifdef __ZEPHYR__` or `#ifdef ZEPHYR_BUILD` |
| GPIO | `gpio_sim::pin_state[]` array | Real STM32 GPIO registers |
| I2C | `i2c_sim::regs[][]` array | Real I2C peripheral |
| SPI | `spi_sim::rx_inject[]` | Real SPI peripheral |
| Timer | `k_timer_fire()` manual call | Real Zephyr kernel timer |
| Build | `cmake -B build && make` | `west build -b nucleo_f446re` |

Driver logic (Observer, Factory, Strategy, RingBuffer) is **identical** in both environments. Only HAL calls differ.

---

## 8. Coverage

### Generate Coverage Report

```bash
cd ~/zephyrproject/hce_drivers

# Clean build with coverage enabled (ON by default)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make -C build -j$(nproc)

# Run all tests then generate HTML report
make -C build coverage
```

### Coverage Output

The `coverage` target:
1. Runs `ctest --output-on-failure` (all test executables)
2. Captures raw coverage with `lcov --capture`
3. Filters out GTest, system, and stub files
4. Generates HTML report with branch coverage

Report location:
```
build/coverage/index.html
```

Open in browser:
```bash
xdg-open build/coverage/index.html   # Linux
open build/coverage/index.html        # macOS
```

### GPIO-Only Coverage

```bash
make -C build gpio_coverage
# Report: build/gpio_coverage/index.html
```

### Coverage Target

Per `task_requirements.md`: **100% line and branch coverage** for all modules.

Coverage flags applied automatically when `ENABLE_COVERAGE=ON` (default):
```
-O0 -g --coverage -fprofile-arcs -ftest-coverage
```

---

## 9. Sensor Verification

### BME280 (I2C addr 0x76)

**Register map used in driver:**

| Register | Address | Function |
|----------|---------|----------|
| REG_TEMP_MSB | 0xFA | Temperature data MSB |
| REG_PRESS_MSB | 0xF7 | Pressure data MSB |
| REG_HUM_MSB | 0xFD | Humidity data MSB |
| REG_CTRL_MEAS | 0xF4 | Control/measurement config |

**Expected values (typical room conditions):**
- Temperature: 20–30 °C
- Pressure: 950–1050 hPa
- Humidity: 20–80%

**CSV format:** `BME,<ts_ms>,<temp_C>,<press_hPa>,<hum_pct>`

### SHTC3 (I2C addr 0x70)

**Register map:**

| Register | Address | Function |
|----------|---------|----------|
| REG_TEMP_H | 0x00 | Temperature data high byte |
| REG_HUM_H | 0x02 | Humidity data high byte |
| CMD_WAKEUP | 0x35 | Wake-up command |

**Expected values:** Similar to BME280 (temperature, humidity).

**CSV format:** `SHT,<ts_ms>,<temp_C>,<hum_pct>`

### LPS22HB (I2C addr 0x5C)

**Register map:**

| Register | Address | Function |
|----------|---------|----------|
| REG_CTRL1 | 0x10 | Control register 1 (ODR config) |
| REG_PRESS_XL | 0x28 | Pressure data XL byte |
| REG_TEMP_L | 0x2B | Temperature data low byte |

**Features:** `configODR(3)` sets 25 Hz output rate. 4-tap moving average filter on pressure.

**Expected values:**
- Pressure: ~1013 hPa (sea level)
- Temperature: 20–30 °C

**CSV format:** `LPS,<ts_ms>,<press_hPa_filtered>,<temp_C>`

### PAV3015 Air Velocity (I2C addr 0x28)

**Register map:**

| Register | Address | Function |
|----------|---------|----------|
| DATA_H | 0x00 | ADC data high byte |
| DATA_L | 0x01 | ADC data low byte |
| STATUS | 0x02 | Data ready status |
| CTRL | 0x03 | Range selection |

**Interpolation table (15 m/s range):** 10-point calibration from raw 409 (0 m/s) to 3686 (15 m/s).

**Checksum:** First byte = sum of remaining bytes.

**Expected values:**
- Still air: ~0 m/s (raw ~409)
- Breathing into sensor: 1–5 m/s

**CSV format:** `PAV,<ts_ms>,<raw_code>,<vel_mps>`

---

## 10. Validation Checklist

### Host-Side Validation (No Hardware)

| # | Check | Command | Status |
|---|-------|---------|--------|
| 1 | GPIO tests pass | `./build/gpio_test` | ✔ Implemented |
| 2 | UART tests pass | `./build/uart_test` | ✔ Implemented |
| 3 | I2C tests pass | `./build/i2c_test` | ✔ Implemented |
| 4 | SPI tests pass | `./build/spi_test` | ✔ Implemented |
| 5 | CRC tests pass | `./build/crc_test` | ✔ Implemented |
| 6 | UART benchmark runs | `./build/uart_bench` | ✔ Implemented |
| 7 | CRC Python loopback | `python3 scripts/loopback_test.py --mock` | ✔ Implemented |
| 8 | Coverage report generated | `make -C build coverage` | ✔ Implemented |
| 9 | 100% line+branch coverage | Check `build/coverage/index.html` | ✔ Target |

### Design Pattern Validation

| # | Pattern | Module | Evidence |
|---|---------|--------|----------|
| 1 | Observer | GPIO | `ButtonEventBus` → `IButtonListener` → `LedToggleListener`, `UartLogListener` |
| 2 | Factory Method | I2C | `SensorFactory::create(SensorKind, addr)` → `std::unique_ptr<ISensor>` |
| 3 | Strategy | SPI | `IFilter` → `MovingAverageFilter`, `MedianFilter`, runtime `setFilter()` |
| 4 | Strategy | CRC | `ICrcStrategy` → `Crc16Ccitt`, `Crc8Maxim`, selected at `FrameCodec` construction |
| 5 | C++20 Concept | I2C | `SensorType_c` concept with `static_assert` on all drivers |
| 6 | Template | UART | `RingBuffer<T,N>` SPSC lock-free template with power-of-two constraint |

### On-Target Validation (Requires Hardware)

| # | Check | Expected Result | Status |
|---|-------|----------------|--------|
| 1 | Zephyr builds for nucleo_f446re | `west build` succeeds | ✔ Implemented |
| 2 | Flash succeeds | `west flash` programs board | ✔ Implemented |
| 3 | UART DMA boot log visible | Banner at 921600 baud | ✔ Active |
| 4 | GPIO observer fires on B1 press | LED toggles, log printed | ⚠️ Code exists, commented out in main.cpp |
| 5 | I2C sensors detected | `i2c scan` returns 0x28, 0x5c, 0x70, 0x76 | ⚠️ Code exists, not linked in Zephyr build |
| 6 | I2C CSV output streams | PAV/BME/LPS/SHT lines on UART | ⚠️ Code exists, not linked in Zephyr build |
| 7 | SPI ADC reads valid data | ADC CSV line on UART | ⚠️ Code exists, not linked in Zephyr build |
| 8 | SPI filter swap via UART | `filter mavg` / `filter median` | ⚠️ Requires shell integration (not linked) |
| 9 | CRC self-test passes on boot | "CRC self-test PASSED" log | ⚠️ Code exists, not linked in Zephyr build |

> **Legend:** ✔ = Fully working · ⚠️ = Code implemented but not active in current Zephyr build
