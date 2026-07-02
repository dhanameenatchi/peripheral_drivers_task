// =============================================================================
// moving_average_filter.cpp — MovingAverageFilter strategy implementation
// =============================================================================
#include "moving_average_filter.hpp"
#include <algorithm>

MovingAverageFilter::MovingAverageFilter(size_t window) : window_(window) {}

int16_t MovingAverageFilter::apply(std::span<const int16_t> samples) {
    if (samples.empty()) return 0;
    size_t n = std::min(samples.size(), window_);
    // Use last n samples
    int32_t sum = 0;
    auto begin = samples.end() - static_cast<ptrdiff_t>(n);
    for (auto it = begin; it != samples.end(); ++it) {
        sum += *it;
    }
    return static_cast<int16_t>(sum / static_cast<int32_t>(n));
}

const char* MovingAverageFilter::name() const {
    return "MovingAverage";
}
