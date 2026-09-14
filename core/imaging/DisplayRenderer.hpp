#ifndef GLCM_DISPLAY_RENDERER_HPP_
#define GLCM_DISPLAY_RENDERER_HPP_

#include <array>
#include <cstdint>
#include <opencv2/core.hpp>

namespace glcm {

struct DisplayStatistics {
    int window_min = 0;                    // 0.5th percentile (nearest rank)
    int window_max = 0;                    // 99.5th percentile (nearest rank)
    std::array<uint64_t, 256> histogram{}; // 256 equal bins over the full range of the bit depth (0-255 or 0-65535)
};

// Default display window and histogram of an 8- or 16-bit single-channel image
DisplayStatistics ComputeDisplayStatistics(const cv::Mat& gray);

// Window/level mapping of one intensity to 8 bits, shared with the browser renderer. With integers min <= max:
// - max == min: 255 if value >= min, otherwise 0
// - otherwise:  clamp(floor(((value - min) * 510 + (max - min)) / (2 * (max - min))), 0, 255)
// The second line is round((value - min) * 255 / (max - min)) with halves rounded up, in integer arithmetic so that
// C++, a JavaScript lookup table and a WebGL2 integer shader give identical results.
uint8_t WindowLevel(int value, int window_min, int window_max);

// 8-bit rendering of an 8- or 16-bit single-channel image with the window [window_min, window_max]. When max_size > 0
// and the long side is larger, the result is downscaled (area averaging) so that the long side equals max_size.
// Throws std::invalid_argument for an invalid image, window (min > max) or max_size < 0.
cv::Mat RenderWindowLevel(const cv::Mat& gray, int window_min, int window_max, int max_size = 0);

} // namespace glcm

#endif // GLCM_DISPLAY_RENDERER_HPP_
