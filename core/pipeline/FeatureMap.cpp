#include "pipeline/FeatureMap.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include "pipeline/AnalysisSettings.hpp"
#include "pipeline/FeatureCatalog.hpp"

namespace glcm {

namespace {

int64_t CeilDivide(int64_t numerator, int64_t denominator) {
    return (numerator + denominator - 1) / denominator;
}

// Centre pixel of block `index` along an axis of `size` pixels
int BlockCentre(int index, int step, int size) {
    const int64_t first = static_cast<int64_t>(index) * step;
    const int64_t last = std::min<int64_t>(first + step, size) - 1;
    return static_cast<int>((first + last) / 2);
}

int PixelValue(const cv::Mat& gray, int row, int col) {
    return (gray.depth() == CV_16U) ? gray.at<uint16_t>(row, col) : gray.at<uchar>(row, col);
}

// Gray levels of the rows [top, bottom) of the image, quantized as if the whole image were the region
cv::Mat QuantizeRows(const cv::Mat& gray, int top, int bottom, const FeatureMapSettings& settings) {
    // Quantize checks the settings against the image's extreme values and gives the range they resolve to
    double min_value = 0.0;
    double max_value = 0.0;
    cv::minMaxLoc(gray, &min_value, &max_value);
    cv::Mat extremes(1, 2, gray.type());
    if (gray.depth() == CV_16U) {
        extremes.at<uint16_t>(0, 0) = static_cast<uint16_t>(min_value);
        extremes.at<uint16_t>(0, 1) = static_cast<uint16_t>(max_value);
    } else {
        extremes.at<uchar>(0, 0) = static_cast<uchar>(min_value);
        extremes.at<uchar>(0, 1) = static_cast<uchar>(max_value);
    }
    const QuantizationResult range =
        Quantize(extremes, cv::Mat(1, 2, CV_8UC1, cv::Scalar(255)), settings.gray_levels, settings.quantization);

    const cv::Mat rows = gray.rowRange(top, bottom);
    const cv::Mat inside(rows.size(), CV_8UC1, cv::Scalar(255));
    switch (settings.quantization.method) {
        case QuantizationMethod::FixedRange:
        case QuantizationMethod::RoiMinMax: {
            QuantizationSettings fixed;
            fixed.method = QuantizationMethod::FixedRange;
            fixed.range_min = range.lower;
            fixed.range_max = range.upper;
            return Quantize(rows, inside, settings.gray_levels, fixed).image;
        }
        case QuantizationMethod::None:
            return Quantize(rows, inside, settings.gray_levels, settings.quantization).image;
        case QuantizationMethod::FixedBinWidth: {
            // Bins start at the image minimum, not at the minimum of these rows (same arithmetic as Quantize)
            cv::Mat levels(rows.size(), CV_8UC1);
            for (int row = 0; row < rows.rows; ++row) {
                for (int col = 0; col < rows.cols; ++col) {
                    levels.at<uchar>(row, col) = static_cast<uchar>(
                        static_cast<int>(std::floor((PixelValue(rows, row, col) - range.lower) / settings.quantization.bin_width)));
                }
            }
            return levels;
        }
    }
    throw std::invalid_argument("Unknown quantization method");
}

} // namespace

bool IsFeatureMapFeature(Type type) {
    if (type == Type::MaximalCorrelationCoefficient) {
        return false;
    }
    for (const FeatureInfo& info : FeatureCatalog()) {
        if (info.type == type) {
            return info.group == FeatureGroup::Haralick || info.group == FeatureGroup::Other;
        }
    }
    return false;
}

int AutomaticFeatureMapStep(int width, int height) {
    const int64_t longest = std::max(std::max(width, height), 1);
    return static_cast<int>(CeilDivide(longest, AUTOMATIC_FEATURE_MAP_POINTS_PER_SIDE));
}

FeatureMapGrid ResolveFeatureMapGrid(int width, int height, int step) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("The image must not be empty");
    }
    if (step < 0) {
        throw std::invalid_argument("The feature map step must be positive, or 0 to choose it automatically");
    }
    FeatureMapGrid grid;
    grid.step = (step == 0) ? AutomaticFeatureMapStep(width, height) : step;
    const int64_t columns = CeilDivide(width, grid.step);
    const int64_t rows = CeilDivide(height, grid.step);
    if (columns > MAX_FEATURE_MAP_POINTS_PER_SIDE || rows > MAX_FEATURE_MAP_POINTS_PER_SIDE) {
        const int smallest = static_cast<int>(CeilDivide(std::max(width, height), MAX_FEATURE_MAP_POINTS_PER_SIDE));
        throw std::invalid_argument("A step of " + std::to_string(grid.step) + " gives more than " +
                                    std::to_string(MAX_FEATURE_MAP_POINTS_PER_SIDE) + " points along a side of this image; use at least " +
                                    std::to_string(smallest));
    }
    grid.columns = static_cast<int>(columns);
    grid.rows = static_cast<int>(rows);
    return grid;
}

void ValidateFeatureMapSettings(const FeatureMapSettings& settings) {
    if (!IsFeatureMapFeature(settings.feature)) {
        const std::string name =
            (settings.feature == Type::Score || settings.feature == Type::Age) ? "This feature" : FindFeature(settings.feature).name;
        throw std::invalid_argument(
            name + " cannot be mapped; choose a co-occurrence feature other than the Maximal Correlation Coefficient");
    }
    if (settings.window < MIN_FEATURE_MAP_WINDOW || settings.window > MAX_FEATURE_MAP_WINDOW || settings.window % 2 == 0) {
        throw std::invalid_argument("The window must be an odd number of pixels between " + std::to_string(MIN_FEATURE_MAP_WINDOW) +
                                    " and " + std::to_string(MAX_FEATURE_MAP_WINDOW));
    }
    if (settings.step < 0) {
        throw std::invalid_argument("The feature map step must be positive, or 0 to choose it automatically");
    }
    if (settings.distance >= settings.window) {
        throw std::invalid_argument("The distance must be smaller than the window");
    }

    AnalysisSettings analysis;
    analysis.features = {settings.feature};
    analysis.gray_levels = settings.gray_levels;
    analysis.quantization = settings.quantization;
    analysis.distances = {settings.distance};
    analysis.directions = settings.directions;
    analysis.log_base = settings.log_base;
    ValidateSettings(analysis);
}

std::vector<float> ComputeFeatureMapRows(const cv::Mat& gray, const FeatureMapSettings& settings, int first_row, int row_count) {
    if (gray.empty() || gray.channels() != 1 || (gray.depth() != CV_8U && gray.depth() != CV_16U)) {
        throw std::invalid_argument("The image must be an 8- or 16-bit single-channel image");
    }
    ValidateFeatureMapSettings(settings);
    const FeatureMapGrid grid = ResolveFeatureMapGrid(gray.cols, gray.rows, settings.step);
    if (first_row < 0 || row_count < 0 || first_row > grid.rows - row_count) {
        throw std::invalid_argument("Rows " + std::to_string(first_row) + " to " + std::to_string(first_row + row_count - 1) +
                                    " are outside the map of " + std::to_string(grid.rows) + " rows");
    }
    std::vector<float> values;
    if (row_count == 0) {
        return values;
    }
    values.reserve(static_cast<size_t>(grid.columns) * static_cast<size_t>(row_count));

    const int half = settings.window / 2;
    const int top = std::max(0, BlockCentre(first_row, grid.step, gray.rows) - half);
    const int bottom = std::min(gray.rows, BlockCentre(first_row + row_count - 1, grid.step, gray.rows) + half + 1);
    const cv::Mat levels = QuantizeRows(gray, top, bottom, settings);

    TextureOptions options;
    options.directions = settings.directions;
    options.log_base = settings.log_base;
    TextureAnalysis analysis(settings.gray_levels, options);
    const std::set<Type> types{settings.feature};

    for (int row = first_row; row < first_row + row_count; ++row) {
        const int centre_y = BlockCentre(row, grid.step, gray.rows);
        const cv::Range rows(std::max(0, centre_y - half) - top, std::min(gray.rows, centre_y + half + 1) - top);
        for (int column = 0; column < grid.columns; ++column) {
            const int centre_x = BlockCentre(column, grid.step, gray.cols);
            const cv::Range columns(std::max(0, centre_x - half), std::min(gray.cols, centre_x + half + 1));
            analysis.ProcessRectImage(levels(rows, columns), settings.distance);

            const bool has_pairs = std::all_of(settings.directions.begin(), settings.directions.end(),
                [&](Direction direction) { return analysis.PairCount(direction) > 0; });
            values.push_back(has_pairs ? static_cast<float>(analysis.Calculate(types).at(settings.feature).Avg())
                                       : std::numeric_limits<float>::quiet_NaN());
        }
    }
    return values;
}

} // namespace glcm
