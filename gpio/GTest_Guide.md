# GPIO Host-Side Development Guide (GTest + Coverage)

This guide is used only for **host-side development**.

Purpose:

* Build GPIO unit tests
* Run GTest
* Generate coverage
* Debug GPIO logic

This does **NOT** flash the STM32 board.

---

# Project Directory

```text
hce_drivers/
├── gpio/
├── gpio_build/
├── CMakeLists.txt
```

---

# Step 1 — Go to Project

```bash
cd ~/zephyrproject/zephyr_hce_task/hce_drivers
```

---

# Step 2 — Remove Previous Build

```bash
rm -rf gpio_build
```

---

# Step 3 — Create Build Folder

```bash
mkdir gpio_build
cd gpio_build
```

---

# Step 4 — Configure CMake

```bash
cmake ..
```

Expected

```
Configuring done
Generating done
Build files have been written to...
```

---

# Step 5 — Build GPIO Test Only

```bash
make gpio_test
```

Expected

```
Built target gpio_test
```

---

# Step 6 — Run All GPIO Tests

```bash
./gpio_test
```

Expected

```
21 tests
21 passed
```

---

# Step 7 — Run Single Test

Example

```bash
./gpio_test --gtest_filter=DebounceFilter.BoundaryAt11ms
```

---

# GPIO Coverage

## Remove Previous Coverage Files

```bash
find . -name "*.gcda" -delete
find . -name "*.gcno" -delete
```

---

## Rebuild

```bash
make gpio_test
```

---

## Run Tests

```bash
./gpio_test
```

---

## Capture Coverage

```bash
lcov \
--capture \
--directory . \
--output-file gpio.info \
--ignore-errors mismatch \
--rc branch_coverage=1
```

---

## Remove External Libraries

```bash
lcov \
--remove gpio.info \
"/usr/*" \
"*/gtest/*" \
"*/zephyr_stubs/*" \
--output-file gpio_filtered.info \
--ignore-errors unused
```

---

## Generate HTML

```bash
genhtml gpio_filtered.info \
--output-directory gpio_coverage \
--branch-coverage
```

---

## Open Report

```bash
xdg-open gpio_coverage/index.html
```

Expected

```
Lines       100%
Functions   ~99–100%
```

A function coverage of **99%** is acceptable if the only uncovered function is the virtual destructor of `IButtonListener`.

---

# Host-Side Completion Checklist

* [ ] CMake config successful
* [ ] GPIO test builds
* [ ] All GTests pass
* [ ] Debounce tests pass (9 ms / 10 ms / 11 ms)
* [ ] Coverage generated
* [ ] Line coverage = 100%
* [ ] Function coverage ≈99–100%
