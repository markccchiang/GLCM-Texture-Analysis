#ifndef GLCM_GRAY_TONE_DIFFERENCE_HPP_
#define GLCM_GRAY_TONE_DIFFERENCE_HPP_

#include <map>
#include <opencv2/core.hpp>
#include <set>
#include <vector>

#include "analysis/TextureAnalysis.hpp"

namespace glcm {

// Neighbourhood gray tone difference matrix of one region and distance, for each gray level (0-based)
struct GrayToneDifferenceMatrix {
    int gray_levels = 0;
    std::vector<int> counts;         // n_i: pixels of the gray level
    std::vector<double> differences; // s_i: sum over those pixels of |i - average gray level of the neighbours|
};

// The feature types computed by ComputeGrayToneDifferenceFeatures
bool IsGrayToneDifferenceFeature(Type type);

// For every pixel inside the mask (255): the average gray level of its neighbours inside the mask at a Chebyshev
// distance of exactly `distance` (a ring, as in PyRadiomics), with gray levels counted from 1. A pixel without such
// neighbours still counts, with a difference of 0 (PyRadiomics). levels is CV_8UC1 with values below gray_levels inside
// the mask. Throws std::invalid_argument for images of the wrong type or size, gray levels out of range, or a distance
// below 1.
GrayToneDifferenceMatrix ComputeGrayToneDifferenceMatrix(const cv::Mat& levels, const cv::Mat& mask, int gray_levels, int distance);

// Coarseness, contrast, busyness, complexity and strength, defined as in PyRadiomics (doc/equations.rst,
// "Neighbourhood gray tone difference features (NGTDM)"), including its values for homogeneous regions. They have no
// direction. All values are NaN for an empty mask. Throws std::invalid_argument as ComputeGrayToneDifferenceMatrix,
// and for a type that IsGrayToneDifferenceFeature does not accept.
std::map<Type, double> ComputeGrayToneDifferenceFeatures(const cv::Mat& levels, const cv::Mat& mask, int gray_levels, int distance,
    const std::set<Type>& types);

} // namespace glcm

#endif // GLCM_GRAY_TONE_DIFFERENCE_HPP_
