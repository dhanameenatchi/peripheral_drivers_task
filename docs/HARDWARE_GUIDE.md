# HCE Peripheral Drivers — Complete Hardware Guide

## NUCLEO-F446RE · Zephyr RTOS · C++20

---

## 1. Project Overview

**Target Board:** STM32 NUCLEO-F446RE (ARM Cortex-M4, 180 MHz, 512 KB Flash, 128 KB SRAM)

**RTOS:** Zephyr RTOS (v4.x) with C++20 support

**Communication Interfaces:**
- **GPIO** — Digital I/O with interrupt-driven debounce (PC13 button, PA5/PA6 LEDs)
- **UART** — USART2 via ST-Link VCP at 921600 baud with DMA TX/RX (PA2/PA3)
- **I2C** — I2C1 bus at 400 kHz (PB8 SCL, PB9 SDA) — shared by 4 sensors
- **SPI** — SPI1 at 4 MHz (PA5/PA6/PA7/PB6) — ADS1118 16-bit ADC
- **DMA** — DMA1 streams 5/6 for USART2 background transfers

**Sensors:**

| Sensor | Interface | I2C Address | Function |
|--------|-----------|-------------|----------|
| BME280 | I2C | 0x76 | Temperature, Pressure, Humidity |
| SHTC3 | I2C | 0x70 | Temperature, Humidity |
| LPS22HB | I2C | 0x5C | CPAP airway pressure, Temperature |
| PAV3015 | I2C | 0x28 | CPAP air velocity (0–15 m/s) |
| ADS1118 | SPI | N/A (CS=PB6) | 16-bit ADC, single-ended + differential |

**Design Patterns Implemented:**
- **Observer Pattern** — GPIO module: `ButtonEventBus` → `LedToggleListener`, `UartLogListener`
- **Factory Method Pattern** — I2C module: `SensorFactory::create(SensorKind, addr)` → `std::unique_ptr<ISensor>`
- **Strategy Pattern** — SPI module: `IFilter` → `MovingAverageFilter`, `MedianFilter` (runtime swap)
- **Strategy Pattern** — CRC module: `ICrcStrategy` → `Crc16Ccitt`, `Crc8Maxim` (build-time selectable)

**Architecture:**
- Header-only driver interfaces in `*/include/`
- Implementations in `*/src/`
- Host-side GTest suites in `*/test/` using mock HAL stubs (`zephyr_stubs/`)
- Zephyr application in `zephyr_app/` with device tree overlay and Kconfig

> **⚠️ Note on FS3000:** Requirement.md specifies an FS3000 flow sensor. The implementation uses a **PAV3015 (DFRobot Gravity PAV3000-series)** instead. The PAV3015 driver includes `readVelocity_mps()`, `validateChecksum()`, and a constexpr interpolation table — fulfilling the same functional requirements as FS3000 but with a different sensor.

---

## 2. Hardware Requirements

### Implemented Hardware

| # | Component | Part / Spec | Quantity |
|---|-----------|-------------|----------|
| 1 | MCU Board | STM32 NUCLEO-F446RE | 1 |
| 2 | Air Velocity Sensor | DFRobot Gravity PAV3015 (SEN0639) | 1 |
| 3 | Temp/Press/Humidity Sensor | BME280 breakout (Adafruit 2652 or equiv.) | 1 |
| 4 | Temp/Humidity Sensor | SHTC3 breakout (Adafruit 4636) | 1 |
| 5 | CPAP Pressure Sensor | LPS22HB breakout (Adafruit 4530) | 1 |
| 6 | 16-bit ADC | ADS1118 SPI breakout | 1 |
| 7 | I2C Pull-up Resistors | 4.7 kΩ | 2 |
| 8 | LED Current-Limit Resistor | 220 Ω | 1 |
| 9 | Yellow LED | 5 mm, standard | 1 |
| 10 | Potentiometer | 10 kΩ (for ADC testing) | 1 |
| 11 | Breadboard | Full-size 830-tie | 1 |
| 12 | Jumper Wires | Male-to-Male + Male-to-Female, 20 cm | ~20 |
| 13 | USB Cable | Micro-USB (for ST-Link) | 1 |

### Planned / Pending Hardware

| Component | Notes |
|-----------|-------|
| FS3000-1005 flow sensor | Requirement.md specifies FS3000; PAV3015 was implemented instead |
| Silicone tubing | For CPAP airway simulation; not required for electrical testing |
| Second push button | Requirement.md mentions "×2 tactile buttons"; only on-board B1 (PC13) is used |
| Second LED module | Requirement.md mentions "×2 LED modules"; green LD2 (PA5) + external yellow (PA6) are implemented |

---

## 3. Pin Mapping

All pin assignments are extracted from `zephyr_app/boards/nucleo_f446re.overlay`, `zephyr_app/config/prj.conf`, and source code.

### GPIO Pins

| Signal | MCU Pin | Arduino Header | Direction | Notes |
|--------|---------|---------------|-----------|-------|
| USER_BUTTON (B1) | PC13 | — (on-board) | Input | Active LOW, internal pull-up, ISR on both edges |
| GREEN_LED (LD2) | PA5 | D13 (CN5) | Output | On-board; **conflicts with SPI1_SCK** |
| YELLOW_LED | PA6 | D12 (CN5) | Output | External, 220 Ω to GND |

Source: `nucleo_f446re.overlay` lines 10–22, `digital_io.hpp` `pins::` namespace.

### UART Pins

| Signal | MCU Pin | Arduino Header | Peripheral | Notes |
|--------|---------|---------------|------------|-------|
| USART2_TX | PA2 | D1 (CN10) | USART2 | ST-Link VCP bridge to USB |
| USART2_RX | PA3 | D0 (CN10) | USART2 | ST-Link VCP bridge to USB |

Baud rate: **921600** (set in overlay: `current-speed = <921600>`)
DMA: TX = DMA1 Stream 6 Channel 4, RX = DMA1 Stream 5 Channel 4

### I2C Pins

| Signal | MCU Pin | Arduino Header | Peripheral | Notes |
|--------|---------|---------------|------------|-------|
| I2C1_SCL | PB8 | D15 (CN5) | I2C1 | 400 kHz, requires 4.7 kΩ pull-up to 3.3V |
| I2C1_SDA | PB9 | D14 (CN5) | I2C1 | 400 kHz, requires 4.7 kΩ pull-up to 3.3V |

Source: `nucleo_f446re.overlay` line 56: `pinctrl-0 = <&i2c1_scl_pb8 &i2c1_sda_pb9>`

### I2C Address Map

| Address | Sensor | DT Node | Compatible |
|---------|--------|---------|------------|
| 0x28 | PAV3015 | `pav3015@28` | `i2c-device` |
| 0x5C | LPS22HB | `lps22hb@5c` | `st,lps22hb-press` |
| 0x70 | SHTC3 | `shtc3@70` | `sensirion,shtc3` |
| 0x76 | BME280 | `bme280@76` | `bosch,bme280` |

### SPI Pins

| Signal | MCU Pin | Arduino Header | Peripheral | Notes |
|--------|---------|---------------|------------|-------|
| SPI1_SCK | PA5 | D13 (CN5) | SPI1 | **Shared with LD2 green LED** |
| SPI1_MISO | PA6 | D12 (CN5) | SPI1 | Also YELLOW_LED |
| SPI1_MOSI | PA7 | D11 (CN5) | SPI1 | |
| SPI1_CS | PB6 | D10 (CN5) | GPIO output | Active LOW for ADS1118 |

SPI frequency: 4 MHz, Mode 1 (CPOL=0, CPHA=1)
Source: `nucleo_f446re.overlay` lines 91–103, `adc_driver.hpp` line 56.

### DMA Channels

| Stream | Channel | Peripheral | Direction |
|--------|---------|------------|-----------|
| DMA1 Stream 6 | Ch 4 | USART2 | TX |
| DMA1 Stream 5 | Ch 4 | USART2 | RX |

Source: `nucleo_f446re.overlay` lines 46–48.

### Power Pins Used

| Pin | Voltage | Header | Usage |
|-----|---------|--------|-------|
| 3.3V | 3.3 V | CN6 Pin 4 | All sensors VCC, I2C pull-ups |
| GND | 0 V | CN6 Pin 6 | All sensors GND, LED cathode |
| 5V | 5 V | CN6 Pin 5 | Not used (all sensors are 3.3 V) |

---

## 4. Wiring Diagrams

### Shared I2C Bus Topology

All four I2C sensors share a single SCL/SDA bus with external pull-ups:

```
3.3V (CN6 pin 4)
  │
  ├──[4.7 kΩ]──┬── SCL bus ── PB8 (D15)
  │             │
  │             ├── BME280 SCL
  │             ├── SHTC3  SCL
  │             ├── LPS22HB SCL
  │             └── PAV3015 SCL (Yellow wire on Gravity cable)
  │
  ├──[4.7 kΩ]──┬── SDA bus ── PB9 (D14)
  │             │
  │             ├── BME280 SDA
  │             ├── SHTC3  SDA
  │             ├── LPS22HB SDA
  │             └── PAV3015 SDA (Blue wire on Gravity cable)
  │
  ├── BME280 VCC ── SHTC3 VCC ── LPS22HB VCC ── PAV3015 VCC (Red)
  │
GND (CN6 pin 6)
  │
  └── BME280 GND ── SHTC3 GND ── LPS22HB GND ── PAV3015 GND (Black)
```

### BME280 (I2C addr 0x76)

| BME280 Pin | Connect To | Notes |
|------------|-----------|-------|
| VCC | 3.3V (CN6 pin 4) | |
| GND | GND (CN6 pin 6) | |
| SDA | PB9 (D14) | Shared I2C bus |
| SCL | PB8 (D15) | Shared I2C bus |
| SDO | GND | Sets address to 0x76 (SDO→3.3V = 0x77) |
| CSB | 3.3V | Forces I2C mode (not SPI) |

### SHTC3 (I2C addr 0x70)

| SHTC3 Pin | Connect To | Notes |
|-----------|-----------|-------|
| VCC | 3.3V (CN6 pin 4) | |
| GND | GND (CN6 pin 6) | |
| SDA | PB9 (D14) | Shared I2C bus |
| SCL | PB8 (D15) | Shared I2C bus |

Address 0x70 is fixed (no address selection pin).

### LPS22HB (I2C addr 0x5C)

| LPS22HB Pin | Connect To | Notes |
|-------------|-----------|-------|
| VCC | 3.3V (CN6 pin 4) | |
| GND | GND (CN6 pin 6) | |
| SDA | PB9 (D14) | Shared I2C bus |
| SCL | PB8 (D15) | Shared I2C bus |
| SDO | GND | Sets address to 0x5C (SDO→3.3V = 0x5D) |
| CS | 3.3V | Forces I2C mode |

### PAV3015 Air Velocity (I2C addr 0x28)

Uses DFRobot Gravity 4-pin connector:

| Gravity Cable | Connect To | Notes |
|---------------|-----------|-------|
| RED (VCC) | 3.3V (CN6 pin 4) | |
| BLACK (GND) | GND (CN6 pin 6) | |
| BLUE (SDA) | PB9 (D14) | Shared I2C bus |
| YELLOW (SCL) | PB8 (D15) | Shared I2C bus |

Orientation: sensing element opening must face **into** the airflow direction.

### ADS1118 ADC (SPI)

| ADS1118 Pin | Connect To | Notes |
|-------------|-----------|-------|
| VDD | 3.3V (CN6 pin 4) | |
| GND | GND (CN6 pin 6) | |
| SCLK | PA5 (D13) | SPI1_SCK |
| DOUT | PA6 (D12) | SPI1_MISO |
| DIN | PA7 (D11) | SPI1_MOSI |
| CS | PB6 (D10) | GPIO, active LOW |
| AIN0 | Potentiometer wiper | Test input |
| AGND | GND | |

**⚠️ PA5 Conflict:** PA5 is both SPI1_SCK and LD2 green LED. Options:
- **Option A:** Remove solder bridge SB21 on NUCLEO to free PA5 for SPI
- **Option B:** Use SPI2 (PB13/PB14/PB15) and update the overlay

### Voltage Divider for ADS1118 Testing

```
3.3V ──[10 kΩ]──┬──[10 kΩ]── GND
                │
               AIN0  → ~1.65 V mid-scale
```

Or connect a 10 kΩ potentiometer wiper to AIN0.

### Button (On-Board)

PC13 (B1) — On-board blue button, no external wiring needed. Active LOW with internal pull-up.

### LEDs

```
PA5 (D13) ── LD2 green LED (on-board, no wiring)

PA6 (D12) ──[220 Ω]──[Yellow LED anode (+)]──[cathode (−)]── GND (CN6 pin 6)
```

### UART (On-Board)

PA2/PA3 are routed through the ST-Link VCP bridge. **No external wiring needed** — just connect the micro-USB cable.

---

## 5. Power Requirements

### Voltage Levels

| Component | Operating Voltage | Notes |
|-----------|------------------|-------|
| NUCLEO-F446RE | 3.3 V logic | Powered via USB (5V → on-board regulator → 3.3V) |
| BME280 | 1.8–3.6 V | Use 3.3V supply |
| SHTC3 | 1.62–3.6 V | Use 3.3V supply |
| LPS22HB | 1.7–3.6 V | Use 3.3V supply |
| PAV3015 | 3.3 V | DFRobot Gravity module |
| ADS1118 | 2.0–5.5 V | Use 3.3V supply for level compatibility |

### I2C Pull-ups

- **Required:** 2× 4.7 kΩ resistors from SCL/SDA to 3.3V
- Some breakout boards include on-board pull-ups; if multiple boards have pull-ups enabled, the effective resistance may be too low. Check with an ohmmeter: effective pull-up should be 1–4.7 kΩ.

### Current Considerations

| Component | Typical Current |
|-----------|----------------|
| STM32F446RE | ~30 mA active |
| BME280 | ~3.6 µA measuring |
| SHTC3 | ~430 µA measuring |
| LPS22HB | ~25 µA @ 25 Hz |
| PAV3015 | ~10 mA |
| ADS1118 | ~250 µA |
| Total | < 50 mA (well within USB 500 mA) |

### 5V Tolerance

- **Do NOT** connect 5V signals to any sensor — all sensors are 3.3V devices
- STM32F446RE GPIO pins are 5V-tolerant on most pins, but I2C/SPI pins used here are 3.3V logic

---

## 6. Hardware Bring-Up Procedure

### Step 1 — Power and Verify ST-Link

1. Connect NUCLEO-F446RE via micro-USB cable
2. Green power LED (LD1) should light up
3. Verify ST-Link is detected:
   ```bash
   lsusb | grep STM
   # Expected: Bus xxx Device xxx: ID 0483:374b STMicroelectronics ST-LINK/V2.1
   ```

### Step 2 — Flash Firmware

```bash
cd ~/zephyrproject/hce_drivers/zephyr_app
west build -b nucleo_f446re -- -DCONF_FILE=config/prj.conf -DDTC_OVERLAY_FILE=boards/nucleo_f446re.overlay
west flash
```

Expected output:
```
-- west flash: using runner openocd
Programmed xxxxx bytes at 0x8000000
```

### Step 3 — Open UART Monitor

```bash
screen /dev/ttyACM0 921600
# Or: minicom -D /dev/ttyACM0 -b 921600
```

### Step 4 — Verify Boot Log

Expected output (current UART-only build):
```
*** Booting Zephyr OS build v4.x.x ***
[00:00:00.001] <inf> hce_main: HCE GPIO-Only Build starting on NUCLEO-F446RE
[00:00:00.001] <inf> hce_main: Zephyr version: 4.x.x
[00:00:00.002] <inf> uart_dma: DMA UART ready: TX=PA2 RX=PA3 @ 921600
[00:00:00.002] <inf> hce_main: GPIO observer ready. Press B1 to test.
```

> **⚠️ Current Build State:** The Zephyr app (`main.cpp`) is currently configured as a **UART-only build**. The GPIO observer (`gpio_app_init()`), I2C sensors, SPI ADC, and CRC app initialization are **commented out** in `main.cpp` and `zephyr_app/CMakeLists.txt`. See Section 7 for details on what is and is not active on hardware.

### Step 5 — Verify GPIO (When Enabled)

When `gpio_app_init()` is uncommented and built:
- Press B1 (blue button on NUCLEO) → yellow LED toggles
- UART log shows: `[T=<ms>] BUTTON PRESSED` / `[T=<ms>] BUTTON RELEASED`

### Step 6 — Verify I2C Sensors (When Enabled)

When I2C app is enabled, use Zephyr shell I2C scan:
```
uart:~$ i2c scan i2c@40005400
```
Expected addresses: `0x28  0x5c  0x70  0x76`

CSV output at configured sample rates:
```
PAV,<ts>,<raw>,<vel_mps>
BME,<ts>,<temp_C>,<press_hPa>,<hum_pct>
LPS,<ts>,<press_hPa>,<temp_C>
SHT,<ts>,<temp_C>,<hum_pct>
```

### Step 7 — Verify SPI ADC (When Enabled)

When SPI app is enabled, CSV output at 100 Hz:
```
ADC,<ts>,<raw>,<filtered>
```

---

## 7. Hardware Validation

### Current Zephyr Build State

The Zephyr application currently has the following module status:

| Module | Zephyr App Status | Host GTest Status |
|--------|------------------|-------------------|
| UART DMA | ✅ Active in `main.cpp` | ✅ Full test suite |
| GPIO Observer | ⚠️ Code exists (`gpio_app.cpp`) but **commented out** in `main.cpp` | ✅ Full test suite |
| I2C Sensors | ⚠️ Code exists (`i2c_app.cpp`) but **not linked** in Zephyr CMakeLists | ✅ Full test suite |
| SPI ADC | ⚠️ Code exists (`spi_app.cpp`) but **not linked** in Zephyr CMakeLists | ✅ Full test suite |
| CRC Frame | ⚠️ Code exists (`crc_app.cpp`) but **not linked** in Zephyr CMakeLists | ✅ Full test suite |

All five modules are **fully implemented and tested on host (PC GTest)**. The Zephyr on-target integration currently only activates the UART DMA module.

### Validating via Host GTest (No Hardware Needed)

All modules can be validated on PC using GTest with mock HAL stubs:

```bash
cd ~/zephyrproject/hce_drivers
cmake -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)
./build/gpio_test && ./build/uart_test && ./build/i2c_test && ./build/spi_test && ./build/crc_test
```

### GPIO Validation (Host)

Tests cover: `ButtonEventBus` subscribe/unsubscribe/notify, `LedToggleListener` toggle, `UartLogListener` message format, debounce boundary at 9ms/10ms/11ms, `DigitalInput`/`DigitalOutput` read/write/toggle.

### UART Validation (Host)

Tests cover: `RingBuffer` empty/full/wrap-around, overflow counter saturation, concurrent SPSC producer-consumer (1000 iterations), `DmaUart` log/drain format, FIFO ordering, partial drain, TX/RX overflow counters, `LogPacket` truncation.

### I2C Validation (Host)

Tests cover: `SensorFactory` creates correct types, BME280/SHTC3/LPS22HB register reads and SI conversion, PAV3015 checksum validation, interpolation at all knots and midpoints, min/max raw code clamp, LPS22HB ODR config and 4-tap averaging, I2C NACK error injection, C++20 concept `static_assert` checks, CSV output format.

### SPI Validation (Host)

Tests cover: `MovingAverageFilter` and `MedianFilter` with all-equal/ramp/impulse inputs, strategy swap mid-stream, SPI error injection (returns `INT16_MIN`), `AdcDriver` channel selection TX capture, history accumulation and clear, no-filter fallback.

### CRC Validation (Host)

Tests cover: CRC16-CCITT known vector ("123456789" → 0x29B1), CRC8-Maxim known vector, `FrameCodec` encode→decode round-trip for both strategies, single-bit error detection, zero-length payload, max 32-byte payload boundary, wrong SOF, truncated buffer, null output buffer, buffer-too-small.

### CRC Python Loopback

```bash
python3 scripts/loopback_test.py --mock
```

Expected: 12/12 tests passed (CRC16 and CRC8 variants of: round-trip, single-bit error, frame drop, max payload, zero payload, plus known vectors).

---

## 8. Troubleshooting

### Board Not Detected / `west flash` Fails

```bash
# Check ST-Link driver:
lsusb | grep STM
# Expected: ID 0483:374b STMicroelectronics ST-LINK/V2.1

# If not found:
sudo apt install openocd stlink-tools   # Linux
```

### I2C Sensor Not Responding (All Zeros or NaN)

- Check pull-up resistors: 4.7 kΩ from SCL→3.3V and SDA→3.3V
- Check VCC is connected to 3.3V (not 5V)
- Check SDA/SCL are not swapped
- Use I2C scan: `i2c scan i2c@40005400` — should list 0x28, 0x5c, 0x70, 0x76
- BME280: verify SDO→GND (for 0x76) and CSB→3.3V (I2C mode)
- LPS22HB: verify SDO→GND (for 0x5C) and CS→3.3V (I2C mode)

### SPI ADC Reads Always 0x7FFF or 0x0000

- Check CS (PB6) is wired to ADS1118 CS pin
- Verify SPI Mode 1 (CPOL=0, CPHA=1) — ADS1118 requirement
- Check SB21 solder bridge is disconnected if using PA5 for SPI1_SCK
- Verify AIN0 has a valid analog input (not floating)

### Serial Monitor Shows Garbage

- Verify baud rate is exactly **921600** (not 115200)
- Check correct port: `ls /dev/ttyACM*` on Linux
- Try `screen /dev/ttyACM0 921600` or PuTTY on Windows

### DMA Failures

- Verify `CONFIG_DMA=y` in prj.conf
- Verify `CONFIG_UART_ASYNC_API=y` in prj.conf
- Verify DMA1 is enabled in overlay: `&dma1 { status = "okay"; };`
- Check DMA channel/stream mapping matches USART2 (Stream 5/6, Channel 4)

### CRC Mismatch

- Verify CRC16-CCITT polynomial is 0x1021 with init 0xFFFF
- Verify CRC8-Maxim polynomial is 0x31 with init 0x00 (reflected)
- Run `python3 scripts/loopback_test.py --mock` to validate Python↔C++ compatibility

### Build Error: `ZEPHYR_BASE not set`

```bash
source ~/zephyrproject/zephyr/zephyr-env.sh
export ZEPHYR_BASE=~/zephyrproject/zephyr
```

### Build Error: Missing GTest

```bash
sudo apt install libgtest-dev cmake
cd /usr/src/gtest && sudo cmake . && sudo make && sudo make install
```

### Device Tree Mismatch

- Verify overlay file is passed: `-DDTC_OVERLAY_FILE=boards/nucleo_f446re.overlay`
- Check for DTS compilation errors in build log
- Pin assignments in overlay must match physical wiring

### Power Issues

- All sensors must be on 3.3V — do NOT use 5V
- If sensors are unstable, check total current draw (should be < 50 mA)
- Ensure GND is shared between all sensor breakouts and NUCLEO board
