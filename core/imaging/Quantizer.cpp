#include "imaging/Quantizer.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace glcm {

namespace {

const uchar INSIDE = 255;

int PixelValue(const cv::Mat& gray, int row, int col) {
    return (gray.depth() == CV_16U) ? gray.at<uint16_t>(row, col) : gray.at<uchar>(row, col);
}

template <typename Fn>
void ForEachMaskPixel(const cv::Mat& gray, const cv::Mat& mask, Fn fn) {
    for (int row = 0; row < gray.rows; ++row) {
        const uchar* mask_line = mask.ptr<uchar>(row);
        for (int col = 0; col < gray.cols; ++col) {
            if (mask_line[col] == INSIDE) {
                fn(row, col, PixelValue(gray, row, col));
            }
        }
    }
}

} // namespace

QuantizationResult Quantize(const cv::Mat& gray, const cv::Mat& mask, int gray_levels, const QuantizationSettings& settings) {
    if (gray.empty() || gray.channels() != 1 || (gray.depth() != CV_8U && gray.depth() != CV_16U)) {
        throw std::invalid_argument("The image must be an 8- or 16-bit single-channel image");
    }
    if (mask.type() != CV_8UC1 || mask.size() != gray.size()) {
        throw std::invalid_argument("The mask must be an 8-bit single-channel image of the same size as the image");
    }
    if (gray_levels < 2 || gray_levels > 256) {
        throw std::invalid_argument("The number of gray levels must be between 2 and 256");
    }

    QuantizationResult result;
    int roi_min = INT_MAX;
    int roi_max = INT_MIN;
    ForEachMaskPixel(gray, mask, [&](int, int, int value) {
        roi_min = std::min(roi_min, value);
        roi_max = std::max(roi_max, value);
        ++result.pixels;
    });

    const bool needs_pixels = settings.method == QuantizationMethod::RoiMinMax || settings.method == QuantizationMethod::FixedBinWidth;
    if (needs_pixels && result.pixels == 0) {
        throw std::invalid_argument("The ROI contains no pixels");
    }

    switch (settings.method) {
        case QuantizationMethod::FixedRange:
            if (settings.range_max < settings.range_min) {
                throw std::invalid_argument("The quantization range maximum must not be below its minimum");
            }
            result.lower = settings.range_min;
            result.upper = settings.range_max;
            break;
        case QuantizationMethod::RoiMinMax:
            result.lower = roi_min;
            result.upper = roi_max;
            break;
        case QuantizationMethod::FixedBinWidth: {
            if (!std::isfinite(settings.bin_width) || settings.bin_width <= 0.0) {
                throw std::invalid_argument("The quantization bin width must be positive");
            }
            result.lower = roi_min;
            result.upper = roi_max;
            const double levels_needed = std::floor((roi_max - roi_min) / settings.bin_width) + 1.0;
            if (levels_needed > gray_levels) {
                throw std::invalid_argument("A bin width of " + std::to_string(settings.bin_width) + " needs " +
                                            std::to_string(static_cast<long long>(levels_needed)) +
                                            " gray levels for this ROI, more than Ng = " + std::to_string(gray_levels));
            }
            break;
        }
        case QuantizationMethod::None:
            if (result.pixels > 0 && roi_max >= gray_levels) {
                throw std::invalid_argument("Pixel value " + std::to_string(roi_max) + " is not below Ng = " + std::to_string(gray_levels) +
                                            "; choose a quantization method");
            }
            result.lower = 0;
            result.upper = gray_levels - 1;
            break;
    }

    result.image = cv::Mat::zeros(gray.size(), CV_8UC1);
    const int64_t span = static_cast<int64_t>(result.upper) - result.lower + 1;
    ForEachMaskPixel(gray, mask, [&](int row, int col, int value) {
        int level = 0;
        switch (settings.method) {
            case QuantizationMethod::FixedRange:
            case QuantizationMethod::RoiMinMax:
                if (value > result.lower) {
                    const int64_t scaled = (static_cast<int64_t>(value) - result.lower) * gray_levels / span;
                    level = static_cast<int>(std::min<int64_t>(scaled, gray_levels - 1));
                }
                break;
            case QuantizationMethod::FixedBinWidth:
                level = static_cast<int>(std::floor((value - result.lower) / settings.bin_width));
                break;
            case QuantizationMethod::None:
                level = value;
                break;
        }
        result.image.at<uchar>(row, col) = static_cast<uchar>(level);
    });

    return result;
}

} // namespace glcm
