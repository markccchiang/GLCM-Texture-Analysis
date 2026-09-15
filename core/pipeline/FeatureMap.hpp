#ifndef GLCM_FEATURE_MAP_HPP_
#define GLCM_FEATURE_MAP_HPP_

#include <opencv2/core.hpp>
#include <set>
#include <vector>

#include "analysis/TextureAnalysis.hpp"
#include "imaging/Quantizer.hpp"

namespace glcm {

constexpr int MIN_FEATURE_MAP_WINDOW = 3;
constexpr int MAX_FEATURE_MAP_WINDOW = 127;
// The automatic step keeps the map within this many points along each side of the image
constexpr int AUTOMATIC_FEATURE_MAP_POINTS_PER_SIDE = 512;
// A step chosen by hand may give up to this many points along each side
constexpr int MAX_FEATURE_MAP_POINTS_PER_SIDE = 2048;

// A co-occurrence feature computed in a square window around points of a grid over the whole image
struct FeatureMapSettings {
    Type feature = Type::Contrast;
    int window = 15; // odd side of the window in pixels; windows are clipped at the image edges
    int step = 0;    // grid spacing in pixels; 0 chooses it automatically (AutomaticFeatureMapStep)
    int gray_levels = 32;
    // Applied to the whole image: RoiMinMax uses the image minimum and maximum, FixedBinWidth starts at the image minimum
    QuantizationSettings quantization;
    int distance = 1;
    std::set<Direction> directions{Direction::H, Direction::V, Direction::LD, Direction::RD};
    LogBase log_base = LogBase::Natural;
};

struct FeatureMapGrid {
    int step = 1;
    int columns = 0; // ceil(width / step)
    int rows = 0;    // ceil(height / step)
};

// Co-occurrence features that can be mapped: the Haralick and other co-occurrence features except the Maximal
// Correlation Coefficient, which is too slow to compute for every window
bool IsFeatureMapFeature(Type type);

// Smallest step that gives at most AUTOMATIC_FEATURE_MAP_POINTS_PER_SIDE points along each side
int AutomaticFeatureMapStep(int width, int height);

// The grid for an image with the given step (0: automatic). Throws std::invalid_argument for a negative step or one that
// gives more than MAX_FEATURE_MAP_POINTS_PER_SIDE points along a side.
FeatureMapGrid ResolveFeatureMapGrid(int width, int height, int step);

// Throws std::invalid_argument with a message describing the first problem found
void ValidateFeatureMapSettings(const FeatureMapSettings& settings);

// Values of rows [first_row, first_row + row_count) of the map, row-major. Grid point (column, row) stands for the image
// block [column * step, (column + 1) * step) × [row * step, (row + 1) * step), clipped to the image; its window is centred
// on the block's centre pixel ((first + last) / 2 on each axis, rounded down) and clipped to the image. The value is the
// feature's mean over the selected directions, computed with TextureAnalysis::ProcessRectImage on the quantized window,
// or NaN when a selected direction has no pixel pairs in the window. The image is quantized as a whole, so the rows of a
// map can be computed separately and give the same values. Throws std::invalid_argument for invalid settings or rows,
// or when the image cannot be quantized with the settings.
std::vector<float> ComputeFeatureMapRows(const cv::Mat& gray, const FeatureMapSettings& settings, int first_row, int row_count);

} // namespace glcm

#endif // GLCM_FEATURE_MAP_HPP_
