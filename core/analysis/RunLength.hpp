#ifndef GLCM_RUN_LENGTH_HPP_
#define GLCM_RUN_LENGTH_HPP_

#include <map>
#include <opencv2/core.hpp>
#include <set>
#include <vector>

#include "analysis/TextureAnalysis.hpp"

namespace glcm {

// Runs of one direction: counts[level * max_length + length - 1] is the number of runs of a gray level (0-based) with a
// length from 1 to max_length
struct RunLengthMatrix {
    int gray_levels = 0;
    int max_length = 0;
    std::vector<int> counts;

    int Count(int level, int length) const {
        return counts[static_cast<size_t>(level * max_length + length - 1)];
    }
};

// The feature types computed by ComputeRunLengthFeatures
bool IsRunLengthFeature(Type type);

// Runs of equal gray levels inside the mask (255) along one direction (not Avg), with the pixel steps of
// TextureAnalysis: H along the row, V along the column, LD = 135° down and to the right, RD = 45° down and to the left.
// Pixels outside the mask end a run. levels is CV_8UC1 with values below gray_levels inside the mask.
// Throws std::invalid_argument for images of the wrong type or size, or gray levels out of range.
RunLengthMatrix ComputeRunLengthMatrix(const cv::Mat& levels, const cv::Mat& mask, int gray_levels, Direction direction);

// Gray level run length matrix features for the given directions (the others hold NaN), defined as in PyRadiomics
// (doc/equations.rst, "Run length features (GLRLM)"): gray levels count from 1, run entropy uses the log base.
// All values are NaN for an empty mask. Throws std::invalid_argument as ComputeRunLengthMatrix, and for a type that
// IsRunLengthFeature does not accept.
std::map<Type, Features> ComputeRunLengthFeatures(const cv::Mat& levels, const cv::Mat& mask, int gray_levels,
    const std::set<Direction>& directions, LogBase log_base, const std::set<Type>& types);

} // namespace glcm

#endif // GLCM_RUN_LENGTH_HPP_
