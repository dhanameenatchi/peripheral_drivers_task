#pragma once
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

#define LOG_MODULE_REGISTER(...)

#define LOG_INF(fmt, ...) do { \
    char log_buf[256]; \
    std::snprintf(log_buf, sizeof(log_buf), fmt, ##__VA_ARGS__); \
    fake_uart::stream << log_buf; \
} while(0)
