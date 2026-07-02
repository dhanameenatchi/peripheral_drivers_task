#pragma once
// =============================================================================
// filter.hpp — Strategy interface for ADC filters
// =============================================================================
#include <cstdint>
#include <span>

struct IFilter {
    virtual ~IFilter() = default;
    virtual int16_t apply(std::span<const int16_t> samples) = 0;
    virtual const char* name() const = 0;
};
