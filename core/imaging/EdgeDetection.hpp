#ifndef GLCM_EDGE_DETECTION_HPP_
#define GLCM_EDGE_DETECTION_HPP_

#include <opencv2/core.hpp>

namespace glcm {

constexpr double MAX_EDGE_SIGMA = 10.0;

enum class EdgeMethod {
    Sobel, // gradient magnitude shown between two values
    Canny  // thin edges found with two hysteresis thresholds
};

// Gradient magnitude of an 8- or 16-bit single-channel image, after smoothing with a Gaussian of `sigma` pixels (0: no
// smoothing): sqrt(gx^2 + gy^2) of the 3x3 Sobel derivatives divided by 8, so that a ramp rising by s per pixel has
// magnitude s. The original intensities are used. CV_32FC1 of the image size; the border is reflected.
// Throws std::invalid_argument for another image type or sigma outside [0, MAX_EDGE_SIGMA].
cv::Mat GradientMagnitude(const cv::Mat& gray, double sigma);

// Percentiles (nearest rank) and maximum of the gradient magnitude, for choosing a window or thresholds. The percentiles
// are taken from a regular grid of at most about a million pixels; the maximum from all pixels.
struct GradientStatistics {
    double p50 = 0.0;
    double p90 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double max = 0.0;
};
GradientStatistics ComputeGradientStatistics(const cv::Mat& gray, double sigma);

// 8-bit edge map of the image, in the gradient magnitude units of GradientMagnitude:
// - Sobel: the magnitude mapped linearly from [low, high] to [0, 255] (rounded, clamped); requires low < high.
// - Canny: 255 on the edges found by Canny's method (L2 gradient, non-maximum suppression, hysteresis with the
//   thresholds low and high), 0 elsewhere; requires 0 <= low <= high. OpenCV's Canny needs 16-bit derivatives, so large
//   derivatives are scaled down first, which rounds the thresholds by at most 1/16 of a unit per unit of scaling.
// When max_size > 0 and the long side is larger, the map is reduced so that the long side equals max_size: by area
// averaging for Sobel, and for Canny a reduced pixel is 255 when any pixel it covers is an edge.
// Throws std::invalid_argument for invalid arguments.
cv::Mat RenderEdgeMap(const cv::Mat& gray, EdgeMethod method, double sigma, double low, double high, int max_size = 0);

} // namespace glcm

#endif // GLCM_EDGE_DETECTION_HPP_
