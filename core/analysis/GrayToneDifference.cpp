#include "analysis/GrayToneDifference.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace glcm {

namespace {

const double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
const uchar INSIDE = 255;
// PyRadiomics' coarseness of a region without any difference
const double HOMOGENEOUS_COARSENESS = 1e6;

void CheckInput(const cv::Mat& levels, const cv::Mat& mask, int gray_levels, int distance) {
    if (levels.empty() || levels.type() != CV_8UC1) {
        throw std::invalid_argument("The gray levels must be a non-empty 8-bit single-channel image");
    }
    if (mask.type() != CV_8UC1 || mask.size() != levels.size()) {
        throw std::invalid_argument("The mask must be an 8-bit single-channel image of the same size as the gray levels");
    }
    if (gray_levels < 1 || gray_levels > 256) {
        throw std::invalid_argument("The number of gray levels must be between 1 and 256");
    }
    if (distance < 1) {
        throw std::invalid_argument("The distance must be at least 1");
    }
}

// Sums over rectangles in constant time: prefix[(r + 1) * (cols + 1) + c + 1] is the sum over rows <= r and columns <= c
class PrefixSums {
public:
    PrefixSums(int rows, int cols) : _rows(rows), _cols(cols), _sums(static_cast<size_t>(rows + 1) * static_cast<size_t>(cols + 1), 0) {}

    void Set(int row, int col, int64_t value) {
        At(row + 1, col + 1) = value + At(row, col + 1) + At(row + 1, col) - At(row, col);
    }

    // Sum over rows [row0, row1] and columns [col0, col1], clamped to the image
    int64_t Sum(int row0, int row1, int col0, int col1) const {
        row0 = std::max(row0, 0);
        col0 = std::max(col0, 0);
        row1 = std::min(row1, _rows - 1);
        col1 = std::min(col1, _cols - 1);
        if (row0 > row1 || col0 > col1) {
            return 0;
        }
        return At(row1 + 1, col1 + 1) - At(row0, col1 + 1) - At(row1 + 1, col0) + At(row0, col0);
    }

private:
    int64_t& At(int row, int col) {
        return _sums[static_cast<size_t>(row) * static_cast<size_t>(_cols + 1) + static_cast<size_t>(col)];
    }
    int64_t At(int row, int col) const {
        return _sums[static_cast<size_t>(row) * static_cast<size_t>(_cols + 1) + static_cast<size_t>(col)];
    }

    int _rows;
    int _cols;
    std::vector<int64_t> _sums;
};

} // namespace

bool IsGrayToneDifferenceFeature(Type type) {
    switch (type) {
        case Type::NgtdmCoarseness:
        case Type::NgtdmContrast:
        case Type::NgtdmBusyness:
        case Type::NgtdmComplexity:
        case Type::NgtdmStrength:
            return true;
        default:
            return false;
    }
}

GrayToneDifferenceMatrix ComputeGrayToneDifferenceMatrix(const cv::Mat& levels, const cv::Mat& mask, int gray_levels, int distance) {
    CheckInput(levels, mask, gray_levels, distance);
    GrayToneDifferenceMatrix matrix;
    matrix.gray_levels = gray_levels;
    matrix.counts.assign(static_cast<size_t>(gray_levels), 0);
    matrix.differences.assign(static_cast<size_t>(gray_levels), 0.0);

    // Gray levels (from 1) and pixel counts of the region, so the ring's sums are a square minus the square inside it
    PrefixSums values(levels.rows, levels.cols);
    PrefixSums pixels(levels.rows, levels.cols);
    for (int row = 0; row < levels.rows; ++row) {
        for (int col = 0; col < levels.cols; ++col) {
            const bool inside = mask.at<uchar>(row, col) == INSIDE;
            const int level = levels.at<uchar>(row, col);
            if (inside && level >= gray_levels) {
                throw std::invalid_argument("A gray level inside the mask is not below the number of gray levels");
            }
            values.Set(row, col, inside ? level + 1 : 0);
            pixels.Set(row, col, inside ? 1 : 0);
        }
    }

    const int d = distance;
    for (int row = 0; row < levels.rows; ++row) {
        for (int col = 0; col < levels.cols; ++col) {
            if (mask.at<uchar>(row, col) != INSIDE) {
                continue;
            }
            const int level = levels.at<uchar>(row, col);
            const int64_t count =
                pixels.Sum(row - d, row + d, col - d, col + d) - pixels.Sum(row - d + 1, row + d - 1, col - d + 1, col + d - 1);
            const int64_t sum =
                values.Sum(row - d, row + d, col - d, col + d) - values.Sum(row - d + 1, row + d - 1, col - d + 1, col + d - 1);
            const double difference = count == 0 ? 0.0 : std::abs(static_cast<double>(level + 1) - static_cast<double>(sum) / static_cast<double>(count));
            ++matrix.counts[static_cast<size_t>(level)];
            matrix.differences[static_cast<size_t>(level)] += difference;
        }
    }
    return matrix;
}

std::map<Type, double> ComputeGrayToneDifferenceFeatures(const cv::Mat& levels, const cv::Mat& mask, int gray_levels, int distance,
    const std::set<Type>& types) {
    CheckInput(levels, mask, gray_levels, distance);
    for (Type type : types) {
        if (!IsGrayToneDifferenceFeature(type)) {
            throw std::invalid_argument(TextureAnalysis::TypeToString(type) + " is not computed by ComputeGrayToneDifferenceFeatures");
        }
    }
    std::map<Type, double> result;
    for (Type type : types) {
        result[type] = NAN_VALUE;
    }

    const GrayToneDifferenceMatrix matrix = ComputeGrayToneDifferenceMatrix(levels, mask, gray_levels, distance);
    double pixels = 0.0;
    std::vector<int> present;
    for (int level = 0; level < gray_levels; ++level) {
        pixels += matrix.counts[static_cast<size_t>(level)];
        if (matrix.counts[static_cast<size_t>(level)] > 0) {
            present.push_back(level);
        }
    }
    if (pixels == 0.0) {
        return result;
    }

    const auto p = [&](int level) { return matrix.counts[static_cast<size_t>(level)] / pixels; };
    const auto s = [&](int level) { return matrix.differences[static_cast<size_t>(level)]; };
    double weighted_differences = 0.0; // sum p_i s_i
    double differences = 0.0;          // sum s_i
    for (int level : present) {
        weighted_differences += p(level) * s(level);
        differences += s(level);
    }
    // Sums over pairs of present gray levels i, j (counted from 1)
    double contrast_sum = 0.0;
    double busyness_divisor = 0.0;
    double complexity_sum = 0.0;
    double strength_sum = 0.0;
    for (int a : present) {
        for (int b : present) {
            const double i = a + 1;
            const double j = b + 1;
            contrast_sum += p(a) * p(b) * (i - j) * (i - j);
            busyness_divisor += std::abs(i * p(a) - j * p(b));
            complexity_sum += std::abs(i - j) * (p(a) * s(a) + p(b) * s(b)) / (p(a) + p(b));
            strength_sum += (p(a) + p(b)) * (i - j) * (i - j);
        }
    }
    const double present_levels = static_cast<double>(present.size());

    for (Type type : types) {
        double& value = result[type];
        switch (type) {
            case Type::NgtdmCoarseness:
                value = weighted_differences != 0.0 ? 1.0 / weighted_differences : HOMOGENEOUS_COARSENESS;
                break;
            case Type::NgtdmContrast:
                value = present.size() > 1 ? contrast_sum * differences / pixels / (present_levels * (present_levels - 1)) : 0.0;
                break;
            case Type::NgtdmBusyness:
                value = busyness_divisor != 0.0 ? weighted_differences / busyness_divisor : 0.0;
                break;
            case Type::NgtdmComplexity:
                value = complexity_sum / pixels;
                break;
            case Type::NgtdmStrength:
                value = differences != 0.0 ? strength_sum / differences : 0.0;
                break;
            default:
                break;
        }
    }
    return result;
}

} // namespace glcm
