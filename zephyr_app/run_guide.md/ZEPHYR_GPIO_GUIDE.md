# GPIO Zephyr Hardware Guide (Build + Flash + Verification)

This guide is used only for **hardware testing** on the NUCLEO-F446RE.

Purpose:

* Build firmware
* Flash STM32
* Verify GPIO functionality
* Validate Observer Pattern
* Verify debounce behavior

This guide does **NOT** run GTest.

---

# Step 1 — Go to Zephyr Application

```bash
cd ~/zephyrproject/zephyr_hce_task/hce_drivers/zephyr_app
```

---

# Step 2 — Remove Previous Build

```bash
rm -rf build
```

Do this whenever:

* CMakeLists.txt changed
* Source files moved
* Header/source split
* Strange build errors occur

---

# Step 3 — Build Firmware

First build

```bash
west build -b nucleo_f446re -- \
  -DCONF_FILE=config/prj.conf```

Later builds

```bash
west build
```

Expected

```
[100%] Built target zephyr
```

---

# Step 4 — Flash Board

```bash
west flash
```

Expected

```
Verified OK
```

---

# Step 5 — Open Serial Terminal

```bash
minicom -D /dev/ttyACM0 -b 921600
```

Exit

```
Ctrl+A
X
```

---

# Expected Boot Output

```
*** Booting Zephyr OS ***

HCE GPIO-Only Build starting

GPIO init

GPIO observer ready

Press B1 to test
```

---

# Hardware Verification

## LED Toggle

Press button

Expected

```
LED ON
```

Press again

```
LED OFF
```

Press again

```
LED ON
```

This confirms `LedToggleListener` is functioning correctly.

---

## UART Logging

Press button

Expected

```
[T=12345 ms] BUTTON PRESSED
```

Release button

Expected

```
[T=12390 ms] BUTTON RELEASED
```

This confirms `UartLogListener` is functioning correctly.

---

## Observer Pattern Verification

One button press should produce:

* LED toggle
* UART message

at the same time.

```
Button
      │
      ▼
ButtonEventBus
      │
      ├────────► LedToggleListener
      │            LED toggles
      │
      └────────► UartLogListener
                   UART prints
```

If both actions happen from one button press, the Observer pattern is working.

---

## Debounce Verification

Tap the button quickly.

Correct

```
BUTTON PRESSED
BUTTON RELEASED
```

Incorrect

```
BUTTON PRESSED
BUTTON PRESSED
BUTTON PRESSED
```

Multiple press events from one physical press indicate the debounce logic is not working correctly.

---

# GPIO Hardware Completion Checklist

## Software

* [ ] DigitalInput complete
* [ ] DigitalOutput complete
* [ ] ButtonEventBus complete
* [ ] Observer Pattern complete
* [ ] LedToggleListener complete
* [ ] UartLogListener complete
* [ ] 10 ms debounce implemented

## Hardware

* [ ] Button interrupt works
* [ ] LED toggles correctly
* [ ] UART messages print correctly
* [ ] Timestamp appears
* [ ] Observer pattern demonstrated
* [ ] Debounce verified

When all checklist items are complete, the GPIO module is ready, and you can proceed to the UART module.
