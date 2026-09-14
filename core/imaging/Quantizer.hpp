#ifndef GLCM_QUANTIZER_HPP_
#define GLCM_QUANTIZER_HPP_

#include <opencv2/core.hpp>

namespace glcm {

enum class QuantizationMethod { FixedRange, RoiMinMax, FixedBinWidth, None };

struct QuantizationSettings {
    QuantizationMethod method = QuantizationMethod::FixedRange;
    int range_min = 0;      // FixedRange: intensity mapped to level 0
    int range_max = 255;    // FixedRange: intensity mapped to level Ng - 1
    double bin_width = 1.0; // FixedBinWidth: intensity width of one gray level, starting at the ROI minimum
};

struct QuantizationResult {
    cv::Mat image;  // CV_8UC1: levels in [0, Ng) inside the mask, 0 outside
    int lower = 0;  // intensity mapped to level 0
    int upper = 0;  // top of the intensity range that was used
    int pixels = 0; // number of mask pixels
};

// Maps the pixels inside the mask (value 255) of an 8- or 16-bit single-channel image to Ng gray levels:
// - FixedRange:    level = floor((v - range_min) * Ng / (range_max - range_min + 1)), clamped to [0, Ng - 1]
// - RoiMinMax:     FixedRange with the minimum and maximum of the pixels inside the mask
// - FixedBinWidth: level = floor((v - ROI minimum) / bin_width); throws if the ROI needs more than Ng levels
// - None:          level = v; throws if a value inside the mask is Ng or more
// Throws std::invalid_argument for an invalid image, mask, Ng (2..256) or settings, or an empty mask with RoiMinMax or
// FixedBinWidth.
QuantizationResult Quantize(const cv::Mat& gray, const cv::Mat& mask, int gray_levels, const QuantizationSettings& settings);

} // namespace glcm

#endif // GLCM_QUANTIZER_HPP_
