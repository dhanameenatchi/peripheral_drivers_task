cmake_minimum_required(VERSION 3.20)
project(hce_drivers CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
# ─── Coverage flags (GCC / Clang) ─────────────────────────────────────────────
option(ENABLE_COVERAGE "Enable gcov coverage instrumentation" ON)
if(ENABLE_COVERAGE)
    add_compile_options(-O0 -g --coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# ─── GTest ────────────────────────────────────────────────────────────────────
find_package(GTest REQUIRED)
include(GoogleTest)

# ─── Shared include paths ─────────────────────────────────────────────────────
set(STUBS_INC ${CMAKE_SOURCE_DIR}/zephyr_stubs/include)

# ─── Helper: add a test executable ───────────────────────────────────────────
function(add_hce_test NAME SRC INCLUDES)
    add_executable(${NAME} ${SRC})
    target_include_directories(${NAME} PRIVATE ${INCLUDES} ${STUBS_INC})
    target_link_libraries(${NAME} PRIVATE GTest::gtest GTest::gtest_main pthread)
    gtest_discover_tests(${NAME})
endfunction()

# ═══════════════════════════════════════════════════════════════════════════════
# GPIO
# ═══════════════════════════════════════════════════════════════════════════════
add_hce_test(gpio_test
    gpio/test/gpio_test.cpp
    "${CMAKE_SOURCE_DIR}/gpio/include"
)
target_sources(gpio_test PRIVATE
    gpio/src/digital_io.cpp
    gpio/src/listeners.cpp
)

# ═══════════════════════════════════════════════════════════════════════════════
# UART
# ═══════════════════════════════════════════════════════════════════════════════
add_hce_test(uart_test
    uart/test/uart_test.cpp
    "${CMAKE_SOURCE_DIR}/uart/include"
)
target_sources(uart_test PRIVATE
    uart/src/dma_uart.cpp
)

# ─── UART Throughput Benchmark ────────────────────────────────────────────────
add_executable(uart_bench
    uart/bench/bench_uart.cpp
    uart/src/dma_uart.cpp
)
target_include_directories(uart_bench PRIVATE
    ${CMAKE_SOURCE_DIR}/uart/include
    ${STUBS_INC}
)
target_link_libraries(uart_bench PRIVATE pthread)
target_compile_options(uart_bench PRIVATE -O2 -g -Wall -Wextra)

add_custom_target(bench
    COMMAND ${CMAKE_BINARY_DIR}/uart_bench
    DEPENDS uart_bench
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running UART throughput benchmark"
)

# ═══════════════════════════════════════════════════════════════════════════════
# I2C
# ═══════════════════════════════════════════════════════════════════════════════
add_hce_test(i2c_test
    i2c/test/test_concepts.cpp
    "${CMAKE_SOURCE_DIR}/i2c/include"
)
target_include_directories(i2c_test PRIVATE
    ${CMAKE_SOURCE_DIR}/i2c/include
)
target_sources(i2c_test PRIVATE
    i2c/test/test_sensor_factory.cpp
    i2c/test/test_bme280.cpp
    i2c/test/test_shtc3.cpp
    i2c/test/test_lps22hb.cpp
    i2c/test/test_pav3015.cpp
    i2c/test/test_error_injection.cpp
    i2c/test/test_csv_output.cpp
    i2c/src/sensor_factory.cpp
    i2c/src/bme280_driver.cpp
    i2c/src/shtc3_driver.cpp
    i2c/src/lps22hb_driver.cpp
    i2c/src/pav3015_driver.cpp
    zephyr_app/src/i2c_app.cpp
)

# ═══════════════════════════════════════════════════════════════════════════════
# SPI
# ═══════════════════════════════════════════════════════════════════════════════
add_hce_test(spi_test
    spi/test/spi_test.cpp
    "${CMAKE_SOURCE_DIR}/spi/include"
)
target_sources(spi_test PRIVATE
    spi/src/moving_average_filter.cpp
    spi/src/median_filter.cpp
    spi/src/adc_driver.cpp
)

# ═══════════════════════════════════════════════════════════════════════════════
# CRC
# ═══════════════════════════════════════════════════════════════════════════════
add_hce_test(crc_test
    crc/test/crc_test.cpp
    "${CMAKE_SOURCE_DIR}/crc/include"
)
target_sources(crc_test PRIVATE
    crc/src/crc16_ccitt.cpp
    crc/src/crc8_maxim.cpp
    crc/src/frame_codec.cpp
)

# ═══════════════════════════════════════════════════════════════════════════════
# Coverage targets
# ═══════════════════════════════════════════════════════════════════════════════
find_program(LCOV     lcov)
find_program(GENHTML  genhtml)

if(LCOV AND GENHTML AND ENABLE_COVERAGE)

    # ── Full coverage (all modules) ──────────────────────────────────────────
    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${LCOV} --capture --directory .
                        --output-file coverage_raw.info
                        --ignore-errors mismatch
                        --rc branch_coverage=1
        COMMAND ${LCOV} --remove coverage_raw.info
                        "*/gtest/*"
                        "/usr/*"
                        "*/zephyr_stubs/*"
                        --output-file coverage_filtered.info
                        --ignore-errors unused
                        --rc branch_coverage=1
        COMMAND ${GENHTML} coverage_filtered.info
                        --output-directory coverage/
                        --branch-coverage
                        --rc branch_coverage=1
        COMMAND ${CMAKE_COMMAND} -E echo
                "=== Coverage report: ${CMAKE_BINARY_DIR}/coverage/index.html ==="
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating lcov HTML coverage report (line + branch)"
    )

    # ── Helper macro: per-module coverage ────────────────────────────────────
    macro(add_module_coverage MODULE_NAME TEST_TARGET EXCLUDE_MODULES)
        add_custom_target(${MODULE_NAME}_coverage
            COMMAND ${CMAKE_BINARY_DIR}/${TEST_TARGET}

            COMMAND ${LCOV}
                --capture
                --directory .
                --output-file ${MODULE_NAME}_raw.info
                --ignore-errors mismatch
                --rc branch_coverage=1

            COMMAND ${LCOV}
                --remove ${MODULE_NAME}_raw.info
                "/usr/*"
                "*/gtest/*"
                "*/zephyr_stubs/*"
                ${EXCLUDE_MODULES}
                --output-file ${MODULE_NAME}.info
                --ignore-errors unused
                --rc branch_coverage=1

            COMMAND ${GENHTML}
                ${MODULE_NAME}.info
                --output-directory ${MODULE_NAME}_coverage
                --branch-coverage
                --rc branch_coverage=1

            COMMAND ${CMAKE_COMMAND} -E echo
                "=== ${MODULE_NAME} Coverage Report ==="
            COMMAND ${CMAKE_COMMAND} -E echo
                "${CMAKE_BINARY_DIR}/${MODULE_NAME}_coverage/index.html"

            DEPENDS ${TEST_TARGET}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating ${MODULE_NAME}-only coverage report"
        )
    endmacro()

    # ── Per-module coverage targets ──────────────────────────────────────────
    add_module_coverage(gpio gpio_test
        "*/uart/*;*/i2c/*;*/spi/*;*/crc/*;*/zephyr_app/*")

    add_module_coverage(uart uart_test
        "*/gpio/*;*/i2c/*;*/spi/*;*/crc/*;*/zephyr_app/*")

    add_module_coverage(i2c i2c_test
        "*/gpio/*;*/uart/*;*/spi/*;*/crc/*")

    add_module_coverage(spi spi_test
        "*/gpio/*;*/uart/*;*/i2c/*;*/crc/*;*/zephyr_app/*")

    add_module_coverage(crc crc_test
        "*/gpio/*;*/uart/*;*/i2c/*;*/spi/*;*/zephyr_app/*")

endif()
