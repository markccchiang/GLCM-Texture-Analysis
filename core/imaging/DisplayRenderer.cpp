#include "imaging/DisplayRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <vector>

namespace glcm {

namespace {

void RequireGrayImage(const cv::Mat& gray) {
    if (gray.empty() || gray.channels() != 1 || (gray.depth() != CV_8U && gray.depth() != CV_16U)) {
        throw std::invalid_argument("The image must be a non-empty 8- or 16-bit single-channel image");
    }
}

// Smallest intensity whose cumulative count reaches the rank
int ValueAtRank(const std::vector<uint64_t>& counts, uint64_t rank) {
    uint64_t cumulative = 0;
    for (size_t value = 0; value < counts.size(); ++value) {
        cumulative += counts[value];
        if (cumulative >= rank) {
            return static_cast<int>(value);
        }
    }
    return static_cast<int>(counts.size()) - 1;
}

} // namespace

DisplayStatistics ComputeDisplayStatistics(const cv::Mat& gray) {
    RequireGrayImage(gray);
    const bool sixteen_bit = gray.depth() == CV_16U;
    std::vector<uint64_t> counts(sixteen_bit ? 65536 : 256, 0);

    for (int row = 0; row < gray.rows; ++row) {
        if (sixteen_bit) {
            const uint16_t* line = gray.ptr<uint16_t>(row);
            for (int col = 0; col < gray.cols; ++col) {
                ++counts[line[col]];
            }
        } else {
            const uchar* line = gray.ptr<uchar>(row);
            for (int col = 0; col < gray.cols; ++col) {
                ++counts[line[col]];
            }
        }
    }

    DisplayStatistics statistics;
    const int shift = sixteen_bit ? 8 : 0;
    for (size_t value = 0; value < counts.size(); ++value) {
        statistics.histogram[value >> shift] += counts[value];
    }

    // Nearest-rank percentiles in integer arithmetic: rank = max(1, ceil(N * p / 1000)) for p = 5 and 995
    const uint64_t total = static_cast<uint64_t>(gray.rows) * gray.cols;
    const uint64_t low_rank = std::max<uint64_t>(1, (total * 5 + 999) / 1000);
    const uint64_t high_rank = std::max<uint64_t>(1, (total * 995 + 999) / 1000);
    statistics.window_min = ValueAtRank(counts, low_rank);
    statistics.window_max = ValueAtRank(counts, high_rank);
    return statistics;
}

uint8_t WindowLevel(int value, int window_min, int window_max) {
    if (window_max == window_min) {
        return (value >= window_min) ? 255 : 0;
    }
    if (value <= window_min) {
        return 0;
    }
    if (value >= window_max) {
        return 255;
    }
    const int64_t width = static_cast<int64_t>(window_max) - window_min;
    const int64_t numerator = (static_cast<int64_t>(value) - window_min) * 510 + width;
    return static_cast<uint8_t>(numerator / (2 * width));
}

cv::Mat RenderWindowLevel(const cv::Mat& gray, int window_min, int window_max, int max_size) {
    RequireGrayImage(gray);
    if (window_min > window_max) {
        throw std::invalid_argument("The window minimum must not be above its maximum");
    }
    if (max_size < 0) {
        throw std::invalid_argument("The maximum display size must not be negative");
    }

    const bool sixteen_bit = gray.depth() == CV_16U;
    std::vector<uchar> lookup(sixteen_bit ? 65536 : 256);
    for (size_t value = 0; value < lookup.size(); ++value) {
        lookup[value] = WindowLevel(static_cast<int>(value), window_min, window_max);
    }

    cv::Mat rendered(gray.size(), CV_8UC1);
    for (int row = 0; row < gray.rows; ++row) {
        uchar* out = rendered.ptr<uchar>(row);
        if (sixteen_bit) {
            const uint16_t* line = gray.ptr<uint16_t>(row);
            for (int col = 0; col < gray.cols; ++col) {
                out[col] = lookup[line[col]];
            }
        } else {
            const uchar* line = gray.ptr<uchar>(row);
            for (int col = 0; col < gray.cols; ++col) {
                out[col] = lookup[line[col]];
            }
        }
    }

    const int long_side = std::max(gray.cols, gray.rows);
    if (max_size > 0 && long_side > max_size) {
        const double scale = static_cast<double>(max_size) / long_side;
        const int width = std::max(1, static_cast<int>(std::lround(gray.cols * scale)));
        const int height = std::max(1, static_cast<int>(std::lround(gray.rows * scale)));
        cv::Mat scaled;
        cv::resize(rendered, scaled, cv::Size(width, height), 0, 0, cv::INTER_AREA);
        return scaled;
    }
    return rendered;
}

} // namespace glcm
