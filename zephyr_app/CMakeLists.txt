# =============================================================================
# zephyr_app/CMakeLists.txt — Full I2C Sensor Build
# Board: NUCLEO-F446RE  (west build -b nucleo_f446re)
# =============================================================================
cmake_minimum_required(VERSION 3.20)

# Find Zephyr — must come before project()
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(hce_i2c VERSION 0.1 LANGUAGES CXX)
# ---------------------------------------------------------------------------
# C++20 for all targets
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/include
    # TODO: Uncomment for other modules when integrating
    # ${CMAKE_CURRENT_SOURCE_DIR}/../gpio/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/../uart/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/../spi/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/../crc/include
)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
target_sources(app PRIVATE
    src/main.cpp
    src/i2c_app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/sensor_factory.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/bme280_driver.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/shtc3_driver.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/lps22hb_driver.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/pav3015_driver.cpp
    # TODO: Uncomment for other modules when integrating
    # src/gpio_app.cpp
    # src/uart_app.cpp
    # src/spi_app.cpp
    # src/crc_app.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/../gpio/src/digital_io.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/../gpio/src/listeners.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/../uart/src/dma_uart.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/bme280_driver.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/lps22hb_driver.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/pav3015_driver.cpp
)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
target_compile_options(app PRIVATE
    -DZEPHYR_BUILD          # switches #ifdef to use real Zephyr HAL
    -Wall
    -Wextra
    -Wpedantic
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
)