#ifndef GLCM_LOCAL_BINARY_PATTERN_HPP_
#define GLCM_LOCAL_BINARY_PATTERN_HPP_

#include <array>
#include <map>
#include <opencv2/core.hpp>
#include <set>

#include "analysis/TextureAnalysis.hpp"

namespace glcm {

// Samples on the circle of a local binary pattern
constexpr int LBP_SAMPLES = 8;
// Codes of the rotation-invariant uniform LBP: 0 to LBP_SAMPLES for uniform patterns, LBP_SAMPLES + 1 for the others
constexpr int LBP_CODES = LBP_SAMPLES + 2;

// The feature types computed by ComputeLocalBinaryPatternFeatures
bool IsLocalBinaryPatternFeature(Type type);

// Rotation-invariant uniform LBP code of the pixel (row, col) of an 8- or 16-bit single-channel image, computed as
// scikit-image's local_binary_pattern(image, 8, radius, 'uniform') (doc/equations.rst, "Local binary pattern features
// (LBP)"): 8 samples on a circle of the radius, offsets rounded to 5 decimals, bilinear interpolation with 0 outside the
// image, a sample is set when it is at least the pixel's value, and a pattern is uniform with at most two changes
// between consecutive samples. Throws std::invalid_argument for other images, a pixel outside the image or a radius
// below 1.
int LocalBinaryPatternCode(const cv::Mat& gray, int row, int col, int radius);

// Number of pixels with each code among the pixels inside the mask (255, the size of box) of gray(box). The samples use
// the whole image, including pixels around the box. Throws std::invalid_argument for other images, a box that is not
// inside the image, a mask of another size or a radius below 1.
std::array<int, LBP_CODES> ComputeLocalBinaryPatternHistogram(const cv::Mat& gray, const cv::Rect& box, const cv::Mat& mask, int radius);

// The fraction of the ROI's pixels with each code, and the entropy (with the log base) and energy of those fractions.
// They have no direction. All values are NaN for an empty mask. Throws std::invalid_argument as
// ComputeLocalBinaryPatternHistogram, and for a type that IsLocalBinaryPatternFeature does not accept.
std::map<Type, double> ComputeLocalBinaryPatternFeatures(const cv::Mat& gray, const cv::Rect& box, const cv::Mat& mask, int radius,
    LogBase log_base, const std::set<Type>& types);

} // namespace glcm

#endif // GLCM_LOCAL_BINARY_PATTERN_HPP_
