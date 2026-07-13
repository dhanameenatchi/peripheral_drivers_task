# =============================================================================
# zephyr_app/CMakeLists.txt — Selectable HCE Firmware Build
# Board: NUCLEO-F446RE  (west build -b nucleo_f446re)
# =============================================================================
cmake_minimum_required(VERSION 3.20)

# ── Select MODULE and CONF_FILE (must come before find_package(Zephyr)) ───────
if(DEFINED CONF_FILE)
    if(CONF_FILE MATCHES "prj_i2c")
        set(MODULE I2C)
    elseif(CONF_FILE MATCHES "prj_spi")
        set(MODULE SPI)
    elseif(CONF_FILE MATCHES "prj_gpio")
        set(MODULE GPIO)
    elseif(CONF_FILE MATCHES "prj_uart")
        set(MODULE UART)
    elseif(CONF_FILE MATCHES "prj_crc")
        set(MODULE CRC)
    elseif(CONF_FILE MATCHES "prj_all")
        set(MODULE ALL)
    endif()

    if(NOT CONF_FILE MATCHES "prj\\.conf")
        set(CONF_FILE "config/prj.conf;${CONF_FILE}" CACHE STRING "Forced inclusion of base prj.conf" FORCE)
    endif()
endif()

if(NOT DEFINED MODULE)
    set(MODULE ALL)
endif()

message(STATUS "Configuring HCE Firmware with MODULE=${MODULE}")

string(TOLOWER ${MODULE} MODULE_LOWER)
if(NOT DEFINED CONF_FILE)
    set(CONF_FILE "config/prj.conf;config/prj_${MODULE_LOWER}.conf")
endif()

if(MODULE STREQUAL "CRC")
    set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/boards/nucleo_f446re.overlay;${CMAKE_CURRENT_SOURCE_DIR}/config/crc.overlay" CACHE STRING "CRC overlay configuration" FORCE)
endif()

# Find Zephyr — must come before project()
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(hce_firmware VERSION 0.1 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++20 for all targets
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Select sources, includes, and compile definitions based on MODULE
# ---------------------------------------------------------------------------
set(MODULE_SOURCES src/main.cpp)
set(MODULE_INCLUDES)
set(MODULE_DEFS)

if(MODULE STREQUAL "GPIO" OR MODULE STREQUAL "ALL")
    list(APPEND MODULE_SOURCES
        src/gpio_app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpio/src/digital_io.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpio/src/listeners.cpp
    )
    list(APPEND MODULE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../gpio/include)
    list(APPEND MODULE_DEFS MODULE_GPIO=1)
endif()

if(MODULE STREQUAL "UART" OR MODULE STREQUAL "ALL")
    list(APPEND MODULE_SOURCES
        src/uart_app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../uart/src/dma_uart.cpp
    )
    list(APPEND MODULE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../uart/include)
    list(APPEND MODULE_DEFS MODULE_UART=1)
endif()

if(MODULE STREQUAL "I2C" OR MODULE STREQUAL "ALL")
    list(APPEND MODULE_SOURCES
        src/i2c_app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/sensor_factory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/bme280_driver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/shtc3_driver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/lps22hb_driver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/src/pav3015_driver.cpp
    )
    list(APPEND MODULE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../i2c/include)
    list(APPEND MODULE_DEFS MODULE_I2C=1)
endif()

if(MODULE STREQUAL "SPI" OR MODULE STREQUAL "ALL")
    list(APPEND MODULE_SOURCES
        src/spi_app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../spi/src/adc_driver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../spi/src/moving_average_filter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../spi/src/median_filter.cpp
    )
    list(APPEND MODULE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../spi/include)
    list(APPEND MODULE_DEFS MODULE_SPI=1)
endif()

if(MODULE STREQUAL "CRC" OR MODULE STREQUAL "ALL")
    list(APPEND MODULE_SOURCES
        src/crc_app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../crc/src/crc16_ccitt.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../crc/src/crc8_maxim.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../crc/src/frame_codec.cpp
    )
    list(APPEND MODULE_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../crc/include)
    list(APPEND MODULE_DEFS MODULE_CRC=1)
endif()

# Apply target properties to 'app'
target_include_directories(app PRIVATE ${MODULE_INCLUDES})
target_sources(app PRIVATE ${MODULE_SOURCES})
target_compile_definitions(app PRIVATE ${MODULE_DEFS})

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