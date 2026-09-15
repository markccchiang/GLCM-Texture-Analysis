#ifndef GLCM_SIZE_ZONE_HPP_
#define GLCM_SIZE_ZONE_HPP_

#include <map>
#include <opencv2/core.hpp>
#include <set>
#include <utility>

#include "analysis/TextureAnalysis.hpp"

namespace glcm {

// Zones of one region: counts[{level, size}] is the number of zones of a gray level (0-based) with a size in pixels.
// Only the sizes that occur are stored, since a zone can be as large as the region.
struct SizeZoneMatrix {
    int gray_levels = 0;
    std::map<std::pair<int, int>, int> counts;
};

// The feature types computed by ComputeSizeZoneFeatures
bool IsSizeZoneFeature(Type type);

// Zones: connected pixels of equal gray level inside the mask (255), where a pixel connects to its eight neighbours
// (PyRadiomics' 2D connectivity). levels is CV_8UC1 with values below gray_levels inside the mask.
// Throws std::invalid_argument for images of the wrong type or size, or gray levels out of range.
SizeZoneMatrix ComputeSizeZoneMatrix(const cv::Mat& levels, const cv::Mat& mask, int gray_levels);

// Gray level size zone matrix features, defined as in PyRadiomics (doc/equations.rst, "Size zone features (GLSZM)"):
// gray levels count from 1, zone entropy uses the log base. Zones have no direction. All values are NaN for an empty
// mask. Throws std::invalid_argument as ComputeSizeZoneMatrix, and for a type that IsSizeZoneFeature does not accept.
std::map<Type, double> ComputeSizeZoneFeatures(const cv::Mat& levels, const cv::Mat& mask, int gray_levels, LogBase log_base,
    const std::set<Type>& types);

} // namespace glcm

#endif // GLCM_SIZE_ZONE_HPP_
