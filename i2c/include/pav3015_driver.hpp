#pragma once
#include "sensor.hpp"
#include <array>

struct PavKnot
{
    uint16_t raw;
    float velocity_mps;
};

// 15 m/s range calibration table (PAV3000-1015 / PAV3015)
static constexpr std::array<PavKnot, 13> PAV3015_KNOTS_15MPS = {{
    {409, 0.00f},
    {1203, 2.00f},
    {1597, 3.00f},
    {1908, 4.00f},
    {2187, 5.00f},
    {2400, 6.00f},
    {2629, 7.00f},
    {2801, 8.00f},
    {3006, 9.00f},
    {3178, 10.00f},
    {3309, 11.00f},
    {3563, 13.00f},
    {3686, 15.00f},
}};

// 7 m/s range calibration table (PAV3000-1005)
static constexpr std::array<PavKnot, 9> PAV3015_KNOTS_7MPS = {{
    {409, 0.00f},
    {915, 1.07f},
    {1522, 2.01f},
    {2066, 3.00f},
    {2523, 3.97f},
    {2908, 4.96f},
    {3256, 5.98f},
    {3572, 6.99f},
    {3686, 7.23f},
}};

class PAV3015Driver final : public ISensor
{
public:
    static constexpr uint8_t DEFAULT_ADDR = 0x28;
static constexpr uint8_t RANGE_7MPS  = 0x08;
static constexpr uint8_t RANGE_15MPS = 0x0C;

    explicit PAV3015Driver(uint8_t addr = DEFAULT_ADDR,
                           uint8_t range = RANGE_15MPS);

    bool setRange(uint8_t range);

    // ISensor
    int32_t readRaw() override;
    float toSI() override;
    uint8_t deviceId() override;

    // Extended API
    float readVelocity_mps();

    static bool validateChecksum(const uint8_t *buf, size_t len);

    static constexpr float interpolate15(uint16_t raw)
    {
        return interpolate_table(raw, PAV3015_KNOTS_15MPS);
    }
    static constexpr float interpolate7(uint16_t raw)
    {
        return interpolate_table(raw, PAV3015_KNOTS_7MPS);
    }

    template <size_t N>
    static constexpr float interpolate_table(uint16_t raw,
                                             const std::array<PavKnot, N> &table)
    {
        if (raw <= table.front().raw)
            return table.front().velocity_mps;
        if (raw >= table.back().raw)
            return table.back().velocity_mps;
        for (size_t i = 1; i < N; ++i)
        {
            if (raw <= table[i].raw)
            {
                float span = static_cast<float>(table[i].raw - table[i - 1].raw);
                float t = static_cast<float>(raw - table[i - 1].raw) / span;
                return table[i - 1].velocity_mps + t * (table[i].velocity_mps - table[i - 1].velocity_mps);
            }
        }
        return table.back().velocity_mps;
    }

private:
    I2cBus bus_;
    uint8_t range_;
    int32_t raw_ = 0;
    bool raw_valid_ = false;
};

static_assert(SensorType_c<PAV3015Driver>, "PAV3015Driver must satisfy Sensor concept");
