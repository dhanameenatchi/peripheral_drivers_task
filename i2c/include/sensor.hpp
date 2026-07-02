#pragma once
// =============================================================================
// sensor.hpp — I2C Sensor C++20 concept + ISensor interface
// =============================================================================

#ifdef ZEPHYR_BUILD
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#else
#include "zephyr_i2c_mock.hpp"
#endif

#include <cstdint>
#include <cstddef>
#include <concepts>

// ---------------------------------------------------------------------------
// ISensor — pure virtual base (runtime polymorphism via unique_ptr)
// ---------------------------------------------------------------------------
struct ISensor {
    virtual ~ISensor()        = default;
    virtual int32_t readRaw() = 0;   // raw ADC / register count
    virtual float   toSI()    = 0;   // primary SI-unit reading
    virtual uint8_t deviceId()= 0;   // I2C 7-bit address
};

// ---------------------------------------------------------------------------
// C++20 Sensor concept — compile-time constraint
// ---------------------------------------------------------------------------
template<typename T>
concept SensorType_c = requires(T t) {
    { t.readRaw()  } -> std::same_as<int32_t>;
    { t.toSI()     } -> std::same_as<float>;
    { t.deviceId() } -> std::same_as<uint8_t>;
};

// ---------------------------------------------------------------------------
// I2cBus — thin wrapper around Zephyr i2c_write_read_dt / i2c_write_dt
// ---------------------------------------------------------------------------
class I2cBus {
public:
    explicit I2cBus(uint8_t addr) : addr_(addr) {}

    bool readRegs(uint8_t reg, uint8_t* buf, size_t len) const {
#ifdef ZEPHYR_BUILD
    const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(i2c_sens));

    if (!device_is_ready(dev)) {
        return false;
    }

    return i2c_write_read(dev, addr_, &reg, 1, buf, len) == 0;
#else
    const void* dev = reinterpret_cast<const void*>(static_cast<uintptr_t>(addr_));
    return i2c_write_read_dt(dev, &reg, 1, buf, len) == 0;
#endif
}

    bool writeReg(uint8_t reg, uint8_t val) const {
#ifdef ZEPHYR_BUILD
    const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(i2c_sens));

    if (!device_is_ready(dev)) {
        return false;
    }

    uint8_t buf[2] = {reg, val};

    return i2c_write(dev, buf, sizeof(buf), addr_) == 0;
#else
    const void* dev = reinterpret_cast<const void*>(static_cast<uintptr_t>(addr_));
    uint8_t buf[2] = {reg, val};
    return i2c_write_dt(dev, buf, 2) == 0;
#endif
}

    bool writeCmd(uint16_t cmd) const {
#ifdef ZEPHYR_BUILD
    const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(i2c_sens));

    if (!device_is_ready(dev)) {
        return false;
    }

    uint8_t buf[2] = {
        static_cast<uint8_t>(cmd >> 8),
        static_cast<uint8_t>(cmd & 0xFF)
    };

    return i2c_write(dev, buf, sizeof(buf), addr_) == 0;
#else
    const void* dev = reinterpret_cast<const void*>(static_cast<uintptr_t>(addr_));
    uint8_t buf[2] = {
        static_cast<uint8_t>(cmd >> 8),
        static_cast<uint8_t>(cmd & 0xFF)
    };
    return i2c_write_dt(dev, buf, 2) == 0;
#endif
}

    bool readBytes(uint8_t* buf, size_t len) const {
#ifdef ZEPHYR_BUILD
    const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(i2c_sens));

    if (!device_is_ready(dev)) {
        return false;
    }

    return i2c_read(dev, buf, len, addr_) == 0;
#else
    const void* dev = reinterpret_cast<const void*>(static_cast<uintptr_t>(addr_));
    return i2c_read_dt(dev, buf, len) == 0;
#endif
}

    [[nodiscard]] uint8_t address() const { return addr_; }

private:
    uint8_t addr_;
};
