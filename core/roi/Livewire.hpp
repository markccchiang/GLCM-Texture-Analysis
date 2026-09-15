#ifndef GLCM_LIVEWIRE_HPP_
#define GLCM_LIVEWIRE_HPP_

#include <array>
#include <opencv2/core.hpp>
#include <vector>

namespace glcm {

// Largest distance between the two points of a livewire segment along either axis, in pixels
constexpr int MAX_LIVEWIRE_SPAN = 1024;
// The search box extends this many pixels beyond the two points (clipped to the image)
constexpr int LIVEWIRE_MARGIN = 32;

// The livewire ("intelligent scissors") path between two pixels of an 8- or 16-bit single-channel image: the cheapest
// 8-connected chain of pixels from `from` to `to`, where entering a pixel costs its step length (1, or sqrt(2) diagonally)
// times 1.05 - g / g_max. g is GradientMagnitude(gray, sigma), and g_max its maximum, inside a box around the two points
// that extends LIVEWIRE_MARGIN pixels beyond them; the path stays in that box. Strong edges are cheap, so the path follows
// them. Returns the centres (x + 0.5, y + 0.5) of the path's pixels from `from` to `to`, keeping only both ends and the
// pixels where the path changes direction. Throws std::invalid_argument for another image type, a point outside the
// image, points farther apart than MAX_LIVEWIRE_SPAN along an axis, or an invalid sigma.
std::vector<std::array<double, 2>> LivewirePath(const cv::Mat& gray, cv::Point from, cv::Point to, double sigma);

} // namespace glcm

#endif // GLCM_LIVEWIRE_HPP_
