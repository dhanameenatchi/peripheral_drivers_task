#pragma once
// =============================================================================
// zephyr/logging/log.h — Host-side GTest stub for Zephyr logging macros
// Captures all log output into a fake UART stringstream for test assertions.
// =============================================================================
#include <sstream>
#include <cstdio>
#include <string>

namespace fake_uart {
    inline std::stringstream stream;
    inline void clear() {
        stream.str("");
        stream.clear();
    }
}

class FakeUart {
public:
    static std::stringstream& stream() {
        return fake_uart::stream;
    }
    static void clear() {
        fake_uart::clear();
    }
    static std::string str() {
        return fake_uart::stream.str();
    }
};

// ---------------------------------------------------------------------------
// Logging macros — all levels write to fake_uart::stream
// ---------------------------------------------------------------------------
#define LOG_MODULE_REGISTER(...)
#define LOG_MODULE_DECLARE(...)

#define LOG_INF(fmt, ...) do { \
    char log_buf[256]; \
    std::snprintf(log_buf, sizeof(log_buf), fmt, ##__VA_ARGS__); \
    fake_uart::stream << log_buf; \
} while(0)

#define LOG_ERR(fmt, ...) do { \
    char log_buf[256]; \
    std::snprintf(log_buf, sizeof(log_buf), fmt, ##__VA_ARGS__); \
    fake_uart::stream << log_buf; \
} while(0)

#define LOG_WRN(fmt, ...) do { \
    char log_buf[256]; \
    std::snprintf(log_buf, sizeof(log_buf), fmt, ##__VA_ARGS__); \
    fake_uart::stream << log_buf; \
} while(0)

#define LOG_DBG(fmt, ...) do { \
    char log_buf[256]; \
    std::snprintf(log_buf, sizeof(log_buf), fmt, ##__VA_ARGS__); \
    fake_uart::stream << log_buf; \
} while(0)

#define LOG_WRN_ONCE(fmt, ...) LOG_WRN(fmt, ##__VA_ARGS__)
