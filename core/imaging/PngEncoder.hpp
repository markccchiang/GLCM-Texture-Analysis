#ifndef GLCM_PNG_ENCODER_HPP_
#define GLCM_PNG_ENCODER_HPP_

#include <opencv2/core.hpp>
#include <optional>
#include <vector>

#include "imaging/ImageHeader.hpp"

namespace glcm {

// PNG of an 8- or 16-bit single-channel image, with a pHYs chunk (pixels per metre, rounded) when a spacing is given,
// so ReadImageSize finds the spacing again. Throws std::runtime_error if the image cannot be encoded.
std::vector<uchar> EncodePng(const cv::Mat& gray, const std::optional<PixelSpacing>& spacing);

} // namespace glcm

#endif // GLCM_PNG_ENCODER_HPP_
