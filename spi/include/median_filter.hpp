#pragma once
// =============================================================================
// median_filter.hpp — MedianFilter strategy declaration
// =============================================================================
#include "filter.hpp"
#include <cstddef>

class MedianFilter final : public IFilter {
public:
    explicit MedianFilter(size_t window = 5);

    int16_t apply(std::span<const int16_t> samples) override;
    const char* name() const override;

private:
    size_t window_;
};
