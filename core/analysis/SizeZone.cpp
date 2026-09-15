#include "analysis/SizeZone.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace glcm {

namespace {

const double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
const uchar INSIDE = 255;
// NumPy's np.spacing(1), which PyRadiomics adds inside the logarithm of the zone entropy
const double EPSILON = std::numeric_limits<double>::epsilon();

void CheckInput(const cv::Mat& levels, const cv::Mat& mask, int gray_levels) {
    if (levels.empty() || levels.type() != CV_8UC1) {
        throw std::invalid_argument("The gray levels must be a non-empty 8-bit single-channel image");
    }
    if (mask.type() != CV_8UC1 || mask.size() != levels.size()) {
        throw std::invalid_argument("The mask must be an 8-bit single-channel image of the same size as the gray levels");
    }
    if (gray_levels < 1 || gray_levels > 256) {
        throw std::invalid_argument("The number of gray levels must be between 1 and 256");
    }
}

} // namespace

bool IsSizeZoneFeature(Type type) {
    switch (type) {
        case Type::GlszmSmallAreaEmphasis:
        case Type::GlszmLargeAreaEmphasis:
        case Type::GlszmGrayLevelNonUniformity:
        case Type::GlszmGrayLevelNonUniformityNormalized:
        case Type::GlszmSizeZoneNonUniformity:
        case Type::GlszmSizeZoneNonUniformityNormalized:
        case Type::GlszmZonePercentage:
        case Type::GlszmGrayLevelVariance:
        case Type::GlszmZoneVariance:
        case Type::GlszmZoneEntropy:
        case Type::GlszmLowGrayLevelZoneEmphasis:
        case Type::GlszmHighGrayLevelZoneEmphasis:
        case Type::GlszmSmallAreaLowGrayLevelEmphasis:
        case Type::GlszmSmallAreaHighGrayLevelEmphasis:
        case Type::GlszmLargeAreaLowGrayLevelEmphasis:
        case Type::GlszmLargeAreaHighGrayLevelEmphasis:
            return true;
        default:
            return false;
    }
}

SizeZoneMatrix ComputeSizeZoneMatrix(const cv::Mat& levels, const cv::Mat& mask, int gray_levels) {
    CheckInput(levels, mask, gray_levels);
    SizeZoneMatrix matrix;
    matrix.gray_levels = gray_levels;

    const auto index = [&](int row, int col) { return static_cast<size_t>(row) * static_cast<size_t>(levels.cols) + static_cast<size_t>(col); };
    std::vector<uchar> visited(static_cast<size_t>(levels.rows) * static_cast<size_t>(levels.cols), 0);
    std::vector<std::pair<int, int>> pending;
    for (int row = 0; row < levels.rows; ++row) {
        for (int col = 0; col < levels.cols; ++col) {
            if (mask.at<uchar>(row, col) != INSIDE || visited[index(row, col)] != 0) {
                continue;
            }
            const uchar level = levels.at<uchar>(row, col);
            if (level >= gray_levels) {
                throw std::invalid_argument("A gray level inside the mask is not below the number of gray levels");
            }
            // Flood fill over the eight neighbours, with an explicit stack so large zones cannot overflow the call stack
            int size = 0;
            visited[index(row, col)] = 1;
            pending.emplace_back(row, col);
            while (!pending.empty()) {
                const auto [r, c] = pending.back();
                pending.pop_back();
                ++size;
                for (int dr = -1; dr <= 1; ++dr) {
                    for (int dc = -1; dc <= 1; ++dc) {
                        const int nr = r + dr;
                        const int nc = c + dc;
                        if ((dr == 0 && dc == 0) || nr < 0 || nr >= levels.rows || nc < 0 || nc >= levels.cols) {
                            continue;
                        }
                        if (mask.at<uchar>(nr, nc) == INSIDE && visited[index(nr, nc)] == 0 && levels.at<uchar>(nr, nc) == level) {
                            visited[index(nr, nc)] = 1;
                            pending.emplace_back(nr, nc);
                        }
                    }
                }
            }
            ++matrix.counts[{level, size}];
        }
    }
    return matrix;
}

std::map<Type, double> ComputeSizeZoneFeatures(const cv::Mat& levels, const cv::Mat& mask, int gray_levels, LogBase log_base,
    const std::set<Type>& types) {
    CheckInput(levels, mask, gray_levels);
    for (Type type : types) {
        if (!IsSizeZoneFeature(type)) {
            throw std::invalid_argument(TextureAnalysis::TypeToString(type) + " is not computed by ComputeSizeZoneFeatures");
        }
    }
    std::map<Type, double> result;
    for (Type type : types) {
        result[type] = NAN_VALUE;
    }

    const SizeZoneMatrix matrix = ComputeSizeZoneMatrix(levels, mask, gray_levels);
    std::vector<double> by_level(static_cast<size_t>(gray_levels), 0.0);
    std::map<int, double> by_size;
    double zones = 0.0;
    double pixels = 0.0;
    double small_areas = 0.0;
    double large_areas = 0.0;
    double low_gray = 0.0;
    double high_gray = 0.0;
    double small_low = 0.0;
    double small_high = 0.0;
    double large_low = 0.0;
    double large_high = 0.0;
    double mean_level = 0.0;
    double mean_size = 0.0;
    for (const auto& [key, count] : matrix.counts) {
        const auto [level, size] = key;
        const double c = count;
        // Gray levels count from 1, as in PyRadiomics and the IBSI
        const double i2 = static_cast<double>(level + 1) * (level + 1);
        const double j2 = static_cast<double>(size) * size;
        zones += c;
        pixels += c * size;
        by_level[static_cast<size_t>(level)] += c;
        by_size[size] += c;
        small_areas += c / j2;
        large_areas += c * j2;
        low_gray += c / i2;
        high_gray += c * i2;
        small_low += c / (i2 * j2);
        small_high += c * i2 / j2;
        large_low += c * j2 / i2;
        large_high += c * i2 * j2;
        mean_level += c * (level + 1);
        mean_size += c * size;
    }
    if (zones == 0.0) {
        return result;
    }
    mean_level /= zones;
    mean_size /= zones;

    double level_variance = 0.0;
    double size_variance = 0.0;
    double entropy = 0.0;
    for (const auto& [key, count] : matrix.counts) {
        const auto [level, size] = key;
        const double p = count / zones;
        level_variance += p * (level + 1 - mean_level) * (level + 1 - mean_level);
        size_variance += p * (size - mean_size) * (size - mean_size);
        entropy -= p * (log_base == LogBase::Two ? std::log2(p + EPSILON) : std::log(p + EPSILON));
    }
    double level_uniformity = 0.0;
    for (double value : by_level) {
        level_uniformity += value * value;
    }
    double size_uniformity = 0.0;
    for (const auto& [size, value] : by_size) {
        size_uniformity += value * value;
    }

    for (Type type : types) {
        double& value = result[type];
        switch (type) {
            case Type::GlszmSmallAreaEmphasis:
                value = small_areas / zones;
                break;
            case Type::GlszmLargeAreaEmphasis:
                value = large_areas / zones;
                break;
            case Type::GlszmGrayLevelNonUniformity:
                value = level_uniformity / zones;
                break;
            case Type::GlszmGrayLevelNonUniformityNormalized:
                value = level_uniformity / (zones * zones);
                break;
            case Type::GlszmSizeZoneNonUniformity:
                value = size_uniformity / zones;
                break;
            case Type::GlszmSizeZoneNonUniformityNormalized:
                value = size_uniformity / (zones * zones);
                break;
            case Type::GlszmZonePercentage:
                value = zones / pixels;
                break;
            case Type::GlszmGrayLevelVariance:
                value = level_variance;
                break;
            case Type::GlszmZoneVariance:
                value = size_variance;
                break;
            case Type::GlszmZoneEntropy:
                value = entropy;
                break;
            case Type::GlszmLowGrayLevelZoneEmphasis:
                value = low_gray / zones;
                break;
            case Type::GlszmHighGrayLevelZoneEmphasis:
                value = high_gray / zones;
                break;
            case Type::GlszmSmallAreaLowGrayLevelEmphasis:
                value = small_low / zones;
                break;
            case Type::GlszmSmallAreaHighGrayLevelEmphasis:
                value = small_high / zones;
                break;
            case Type::GlszmLargeAreaLowGrayLevelEmphasis:
                value = large_low / zones;
                break;
            case Type::GlszmLargeAreaHighGrayLevelEmphasis:
                value = large_high / zones;
                break;
            default:
                break;
        }
    }
    return result;
}

} // namespace glcm
