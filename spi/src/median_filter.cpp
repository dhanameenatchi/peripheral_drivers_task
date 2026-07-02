// =============================================================================
// median_filter.cpp — MedianFilter strategy implementation
// =============================================================================
#include "median_filter.hpp"
#include <algorithm>
#include <array>

MedianFilter::MedianFilter(size_t window) : window_(window | 1) {} // force odd

int16_t MedianFilter::apply(std::span<const int16_t> samples) {
    if (samples.empty()) return 0;
    size_t n = std::min(samples.size(), window_);
    // Copy last n to local buffer
    std::array<int16_t, 32> tmp{};
    if (n > tmp.size()) n = tmp.size();
    auto begin = samples.end() - static_cast<ptrdiff_t>(n);
    std::copy(begin, samples.end(), tmp.begin());
    std::sort(tmp.begin(), tmp.begin() + static_cast<ptrdiff_t>(n));
    return tmp[n / 2];
}

const char* MedianFilter::name() const {
    return "Median";
}
