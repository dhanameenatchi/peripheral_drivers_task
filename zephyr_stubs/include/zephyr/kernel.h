#pragma once
#include <cstdint>

inline uint32_t mock_uptime_ms = 0;

inline uint32_t k_uptime_get_32() {
    return mock_uptime_ms;
}
