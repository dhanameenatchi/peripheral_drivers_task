# HCE Peripheral Drivers — Complete Test & Verification Checklist

This document maps all project requirements defined in `docs/task_requirements.md` to their verification results in unit tests and hardware compilation verification.

---

## 1. GPIO — Observer Event Bus

| Requirement | Verification Detail | Status |
| :--- | :--- | :---: |
| **DigitalInput / DigitalOutput C++20 wrappers** | Verified via `DigitalOutput` state, write, toggle tests and `DigitalInput` read tests. | **PASS** |
| **ButtonEventBus observer list** | Tested subscriber count, subscribe, unsubscribe, and notification logic up to capacity limit of 8. | **PASS** |
| **LedToggleListener & UartLogListener** | Verified LED toggling on Pressed only, and UART log message formatting with millisecond timestamps. | **PASS** |
| **10 ms Debounce via Timer** | Verified boundary conditions at 9 ms (suppressed), 10 ms (fired), and 11 ms (fired twice). | **PASS** |
| **Dual LEDs (Green PB0 + Yellow PB1)** | Verified both green and yellow LED observer instances register to the event bus and toggle. | **PASS** |

---

## 2. UART — Ring-Buffer & DMA

| Requirement | Verification Detail | Status |
| :--- | :--- | :---: |
| **Lock-Free SPSC RingBuffer template** | Tested empty/full states, wrap-around push/pop operations, and concurrent thread-safe producer-consumer runs. | **PASS** |
| **Saturating Overflow Counter** | Verified overflow increments and saturates at maximum capacity without buffer corruption. | **PASS** |
| **921600 Baud DMA Configuration** | Verified in `nucleo_f446re.overlay` (`current-speed = <921600>`) and compiled successfully. | **PASS** |
| **Structured Log Packet Format** | Tested `LogPacket::make` formatting, byte output encoding, and transmission timing. | **PASS** |
| **Throughput Benchmark** | Compared CPU-blocking vs DMA throughput under 1 kHz transmission rate. | **PASS** |

---

## 3. I2C — Sensor Factory & Drivers

| Requirement | Verification Detail | Status |
| :--- | :--- | :---: |
| **Sensor C++20 Concept** | Compiler checks verify `readRaw()`, `toSI()`, and `deviceId()` presence via `static_assert`. | **PASS** |
| **BME280 & SHTC3 Drivers** | Verified I2C address passing, reading registers, conversions, and NACK recovery logic. | **PASS** |
| **SensorFactory creation** | Factory successfully returns `std::unique_ptr<ISensor>` with correct runtime type allocation. | **PASS** |
| **LPS22HbDriver 25 Hz Pressure** | Verified register reads, ODR configuration, Pa/hPa conversions, and 4-tap average filtering. | **PASS** |
| **PAV3015Driver 50 Hz Flow** | Replaced FS3000 sensor. Verified velocity interpolation across knots and clamp bounds. | **PASS** |

---

## 4. SPI — ADC Driver & Filter Strategy

| Requirement | Verification Detail | Status |
| :--- | :--- | :---: |
| **AdcDriver C++20 wrapper** | Verified single-ended/differential SPI command generation and read operations. | **PASS** |
| **IFilter Strategy interface** | Swappable filter strategies (`MovingAverageFilter` vs `MedianFilter`) verified at runtime. | **PASS** |
| **MovingAverageFilter (window N)** | Tested response to ramp, impulse, and constant signals. | **PASS** |
| **MedianFilter (window N)** | Tested with impulse noise suppression and in-place sorting correctness. | **PASS** |
| **Runtime Strategy Swap via UART** | Command parsing ("filter mavg" / "filter median") verified in unit tests and compiles. | **PASS** |

---

## 5. CRC-Protected Frame Protocol

| Requirement | Verification Detail | Status |
| :--- | :--- | :---: |
| **ICrcStrategy Strategy Pattern** | Swappable CRC strategies (`Crc16Ccitt` vs `Crc8Maxim`) verified with known vectors. | **PASS** |
| **FrameCodec encode/decode** | Verified round-trip matching, zero-length payloads, and 32-byte maximum boundaries. | **PASS** |
| **Single-Bit Error Detection** | Verified corruption of frame bytes correctly rejects the package. | **PASS** |

---

## 6. Target Hardware Compilation (west build)

| Demo Selection | FLASH Size (Used %) | RAM Size (Used %) | Build Result |
| :--- | :--- | :--- | :---: |
| **DEMO_MODE=ALL** | 94,480 B (18.02%) | 37,964 B (28.96%) | **SUCCESS** |
| **DEMO_MODE=GPIO_ONLY** | 78,496 B (14.97%) | 25,036 B (19.10%) | **SUCCESS** |
