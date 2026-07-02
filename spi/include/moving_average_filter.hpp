#pragma once
// =============================================================================
// moving_average_filter.hpp — MovingAverageFilter strategy declaration
// =============================================================================
#include "filter.hpp"
#include <cstddef>

class MovingAverageFilter final : public IFilter {
public:
    explicit MovingAverageFilter(size_t window = 8);

    int16_t apply(std::span<const int16_t> samples) override;
    const char* name() const override;

private:
    size_t window_;
};
