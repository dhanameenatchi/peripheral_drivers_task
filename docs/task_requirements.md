Task Name	HCE: Peripheral Drivers & Communication Layer: GPIO Observer · UART DMA · I2C Factory · SPI Strategy · CRC Frame [DP][UT]
Problem Statement	Objectives:
GPIO — Observer Event Bus:
Write C++20 DigitalInput / DigitalOutput classes wrapping the Zephyr GPIO HAL on NUCLEO-F446RE. Apply the Observer pattern: ButtonEventBus maintains a list of IButtonListener subscribers; on each debounced press it notifies all registered listeners. Two concrete observers: LedToggleListener and UartLogListener. Use constexpr pin mappings, enum class GPIO state, and a k_timer ISR for 10 ms debounce.

UART — Ring-Buffer + DMA:
Implement a lock-free SPSC RingBuffer<T,N> C++20 template for UART TX/RX. Configure Zephyr DMA for background transfers. Demonstrate structured log packets (timestamp + message) at 921600 baud without CPU blocking. Handle overflow gracefully with a saturating error counter.

I2C — Sensor Factory · LPS22HB · FS3000:
Three I2C sub-tasks. (1) Factory Method: define a Sensor C++20 concept (requires readRaw(), toSI(), deviceId()); implement BME280Driver (I2C addr 0x76) and SHTC3Driver (I2C addr 0x70) on the same shared I2C bus (single SCL/SDA line); each driver receives its 7-bit address at construction; SensorFactory::create(SensorType, i2c_addr) returns std::unique_ptr<ISensor>; sample both at 10 Hz, transmit CSV over UART. (2) LPS22HbDriver: readPressure_hPa(), readTemp_C(), configODR(); sample at 25 Hz, 4-tap moving average, output Pa + °C CSV. (3) Fs3000Driver: readVelocity_mps(), validateChecksum(); sample at 50 Hz, constexpr interpolation table. Medical context: CPAP airway pressure + respiratory flow sensing.

SPI — ADC Driver + Strategy Filter:
Drive ADS1118 16-bit ADC over SPI using C++20 AdcDriver. Apply the Strategy pattern: IFilter interface with apply(std::span<int16_t>) → int16_t; two concrete strategies: MovingAverageFilter and MedianFilter; strategy swappable at runtime via UART command. Output raw + filtered values over UART.

CRC-Protected Frame Protocol:
Define binary frame: [SOF 0xAA | Len | Cmd | Payload ≤32 B | CRC]. Apply the Strategy pattern for checksum: ICrcStrategy interface with Crc16Ccitt (poly 0x1021) and Crc8Maxim (poly 0x31). FrameCodec holds an ICrcStrategy reference selectable at build time. Python host script performs loopback test with single-bit error injection. Medical context: IEC 60601-1-8 alarm communication frame integrity.

Design Pattern(s):
Factory Method Pattern, Observer Pattern, Strategy Pattern

Tasks:
[GPIO]
Design Pattern: Observer Pattern
1. DigitalInput / DigitalOutput C++20 classes — Zephyr GPIO HAL wrapper; constexpr pin mappings; enum class GPIO state
2. ButtonEventBus — IButtonListener subscriber list; subscribe() / unsubscribe() / notify()
3. LedToggleListener and UartLogListener concrete observer classes
4. 10 ms debounce via k_timer ISR; demo log showing press events at 9 ms / 10 ms / 11 ms boundary

[UART]
Design Pattern: Template Metaprogramming (RingBuffer<T,N>)
5. RingBuffer<T,N> — lock-free SPSC C++20 template; push / pop / size; saturating overflow error counter
6. DMA UART driver — background TX/RX at 921600 baud; structured log packet {timestamp_ms, level, char message(32)}
7. Throughput benchmark: CPU-blocking vs DMA comparison at 1 kHz log rate; overflow error injection test

[I2C]
Design Pattern: Factory Method Pattern
8. Sensor C++20 concept — requires readRaw(), toSI(), deviceId(); static_assert constraint checks
9. BME280Driver — I2C addr 0x76; shared bus with SHTC3 (same SCL/SDA); temperature, pressure, humidity at 10 Hz; SI conversion
10. SHTC3Driver — I2C addr 0x70; shared bus with BME280 (same SCL/SDA); temperature, humidity at 10 Hz; SI conversion
11. SensorFactory::create(SensorType, i2c_addr) → std::unique_ptr<ISensor>; address passed at construction; dual-sensor live CSV stream over UART
12. LPS22HbDriver — readPressure_hPa(), readTemp_C(), configODR(); 4-tap moving average filter
13. 25 Hz pressure + temperature CSV log (columns: timestamp_ms, pressure_hPa, temp_C)
14. Averaging correctness demo: ramp input verified against expected average; impulse response check
15. Fs3000Driver — readVelocity_mps(), validateChecksum(); constexpr interpolation table (std::array of knot pairs)
16. 50 Hz flow velocity CSV log (columns: timestamp_ms, raw_code, velocity_mps)
17. Interpolation correctness: verified at all table knots and midpoints; min/max raw code clamp test

[SPI]
Design Pattern: Strategy Pattern
18. AdcDriver — C++20 class driving ADS1118 16-bit ADC; SPI Mode 1; single-ended + differential channels
19. IFilter interface — apply(std::span<int16_t>) → int16_t; strategy pointer swappable at runtime
20. MovingAverageFilter concrete strategy (window N configurable at construction)
21. MedianFilter concrete strategy (odd window N, in-place sort)
22. Runtime strategy swap via UART command; side-by-side raw vs filtered output log

[CRC]
Design Pattern: Strategy Pattern
23. ICrcStrategy interface — compute(std::span<const uint8_t>) → uint16_t
24. Crc16Ccitt concrete strategy (poly 0x1021, init 0xFFFF)
25. Crc8Maxim concrete strategy (poly 0x31, init 0x00)
26. FrameCodec — encode / decode {SOF 0xAA | Len 1B | Cmd 1B | Payload ≤32B | CRC}; Python loopback script with single-bit error injection and frame drop simulation

Test Suites:
[GPIO] ButtonEventBus subscribe/unsubscribe/notify; LedToggleListener state toggle; UartLogListener message format; debounce boundary (9 ms / 10 ms / 11 ms). 100% coverage report.

[UART] RingBuffer empty/full/wrap-around push-pop; overflow counter saturation; concurrent producer-consumer simulation with 1 000 iterations. 100% coverage report.

[I2C] SensorFactory creates correct concrete type per SensorType enum; SI conversion boundary values; I2C NACK error injection; concept constraint static_assert; LPS22HB register-map mock read + ODR config byte; hPa→Pa conversion; averaging with ramp input; Fs3000 checksum valid/invalid; interpolation at table knots + midpoints; min/max raw code clamp. 100% coverage report.

[SPI] Each filter with all-equal / ramp / impulse input; strategy swap mid-stream preserves continuity; SPI read error injection; AdcDriver channel selection. 100% coverage report.

[CRC] FrameCodec encode→decode round-trip; single-bit error detection; each CRC strategy against known test vectors; max-payload (32 B) boundary; frame with zero-length payload. 100% coverage report.

Unit Testing:
GTest + gcov/lcov — 100% line and branch coverage
CI target: 'make coverage' must pass with zero uncovered lines
Stub all ISR / callback dependencies for host-side test builds
GTest mocks use static global instances only (no heap)
Task Category1	Advanced
Task State	Active
Complexity Level	Easy
Required skills	Python
Paper/ Spec grokking
Ground-up dev
Electronic testing/bring up
Design Patterns (GoF/C++20)
RTOS (Zephyr)
Comms Protocols
Embedded & RT Firmware Dev
Algo Data Processing
Data Acq & Logging
HW-SW Co-Design
HW Proto & Assembly
Hardware Engineering
Unit Testing / TDD
Firmware Debug & Tracing
Build System & Toolchain
Assets Receivable	[GPIO]
NUCLEO-F446RE
Tactile push buttons ×2 
Single-colour LED modules ×2 
Jumper wires


[UART]
NUCLEO-F446RE
PC with serial terminal
USB cable (Type-A to Micro-B)

[I2C]
NUCLEO-F446RE
BME280 breakout 
SHTC3 breakout 
LPS22HB breakout board 
FS3000-1005 flow sensor (or use alternate flow sensor after research)
silicone tube 
Pull-up resistors 4.7 kΩ ×2 

[SPI]
NUCLEO-F446RE
ADS1118 ADC breakout 
Potentiometer 10 kΩ 
Jumper wires

[CRC]
NUCLEO-F446RE
USB cable (Type-A to Micro-B)