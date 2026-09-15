#include "analysis/RunLength.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace glcm {

namespace {

const double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
const uchar INSIDE = 255;
// NumPy's np.spacing(1), which PyRadiomics adds inside the logarithm of the run entropy
const double EPSILON = std::numeric_limits<double>::epsilon();

struct Step {
    int row;
    int col;
};

// The pixel steps of TextureAnalysis's neighbour offsets
Step StepOf(Direction direction) {
    switch (direction) {
        case Direction::H:
            return {0, 1};
        case Direction::V:
            return {1, 0};
        case Direction::LD:
            return {1, 1};
        case Direction::RD:
            return {1, -1};
        default:
            throw std::invalid_argument("Run lengths need one of the four directions");
    }
}

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

double& ValueOf(Features& features, Direction direction) {
    switch (direction) {
        case Direction::H:
            return features.H;
        case Direction::V:
            return features.V;
        case Direction::LD:
            return features.LD;
        default:
            return features.RD;
    }
}

} // namespace

bool IsRunLengthFeature(Type type) {
    switch (type) {
        case Type::GlrlmShortRunEmphasis:
        case Type::GlrlmLongRunEmphasis:
        case Type::GlrlmGrayLevelNonUniformity:
        case Type::GlrlmGrayLevelNonUniformityNormalized:
        case Type::GlrlmRunLengthNonUniformity:
        case Type::GlrlmRunLengthNonUniformityNormalized:
        case Type::GlrlmRunPercentage:
        case Type::GlrlmGrayLevelVariance:
        case Type::GlrlmRunVariance:
        case Type::GlrlmRunEntropy:
        case Type::GlrlmLowGrayLevelRunEmphasis:
        case Type::GlrlmHighGrayLevelRunEmphasis:
        case Type::GlrlmShortRunLowGrayLevelEmphasis:
        case Type::GlrlmShortRunHighGrayLevelEmphasis:
        case Type::GlrlmLongRunLowGrayLevelEmphasis:
        case Type::GlrlmLongRunHighGrayLevelEmphasis:
            return true;
        default:
            return false;
    }
}

RunLengthMatrix ComputeRunLengthMatrix(const cv::Mat& levels, const cv::Mat& mask, int gray_levels, Direction direction) {
    CheckInput(levels, mask, gray_levels);
    const Step step = StepOf(direction);
    RunLengthMatrix matrix;
    matrix.gray_levels = gray_levels;
    matrix.max_length = std::max(levels.rows, levels.cols);
    matrix.counts.assign(static_cast<size_t>(gray_levels) * static_cast<size_t>(matrix.max_length), 0);

    const auto inside = [&](int row, int col) {
        return row >= 0 && row < levels.rows && col >= 0 && col < levels.cols && mask.at<uchar>(row, col) == INSIDE;
    };
    for (int row = 0; row < levels.rows; ++row) {
        for (int col = 0; col < levels.cols; ++col) {
            if (!inside(row, col)) {
                continue;
            }
            const uchar level = levels.at<uchar>(row, col);
            if (level >= gray_levels) {
                throw std::invalid_argument("A gray level inside the mask is not below the number of gray levels");
            }
            // Each run is counted once, from its first pixel
            if (inside(row - step.row, col - step.col) && levels.at<uchar>(row - step.row, col - step.col) == level) {
                continue;
            }
            int length = 1;
            while (inside(row + length * step.row, col + length * step.col) && levels.at<uchar>(row + length * step.row, col + length * step.col) == level) {
                ++length;
            }
            ++matrix.counts[static_cast<size_t>(level * matrix.max_length + length - 1)];
        }
    }
    return matrix;
}

std::map<Type, Features> ComputeRunLengthFeatures(const cv::Mat& levels, const cv::Mat& mask, int gray_levels,
    const std::set<Direction>& directions, LogBase log_base, const std::set<Type>& types) {
    CheckInput(levels, mask, gray_levels);
    for (Type type : types) {
        if (!IsRunLengthFeature(type)) {
            throw std::invalid_argument(TextureAnalysis::TypeToString(type) + " is not computed by ComputeRunLengthFeatures");
        }
    }
    std::map<Type, Features> result;
    for (Type type : types) {
        result[type] = Features{NAN_VALUE, NAN_VALUE, NAN_VALUE, NAN_VALUE};
    }

    double pixels = 0.0;
    for (int row = 0; row < mask.rows; ++row) {
        const uchar* line = mask.ptr<uchar>(row);
        for (int col = 0; col < mask.cols; ++col) {
            pixels += line[col] == INSIDE ? 1.0 : 0.0;
        }
    }

    for (Direction direction : directions) {
        if (direction == Direction::Avg) {
            continue;
        }
        const RunLengthMatrix matrix = ComputeRunLengthMatrix(levels, mask, gray_levels, direction);
        std::vector<double> by_level(static_cast<size_t>(gray_levels), 0.0);
        std::vector<double> by_length(static_cast<size_t>(matrix.max_length), 0.0);
        double runs = 0.0;
        double short_runs = 0.0;
        double long_runs = 0.0;
        double low_gray = 0.0;
        double high_gray = 0.0;
        double short_low = 0.0;
        double short_high = 0.0;
        double long_low = 0.0;
        double long_high = 0.0;
        double mean_level = 0.0;
        double mean_length = 0.0;
        for (int level = 0; level < gray_levels; ++level) {
            for (int length = 1; length <= matrix.max_length; ++length) {
                const int count = matrix.Count(level, length);
                if (count == 0) {
                    continue;
                }
                const double c = count;
                // Gray levels count from 1, as in PyRadiomics and the IBSI
                const double i2 = static_cast<double>(level + 1) * (level + 1);
                const double j2 = static_cast<double>(length) * length;
                runs += c;
                by_level[static_cast<size_t>(level)] += c;
                by_length[static_cast<size_t>(length - 1)] += c;
                short_runs += c / j2;
                long_runs += c * j2;
                low_gray += c / i2;
                high_gray += c * i2;
                short_low += c / (i2 * j2);
                short_high += c * i2 / j2;
                long_low += c * j2 / i2;
                long_high += c * i2 * j2;
                mean_level += c * (level + 1);
                mean_length += c * length;
            }
        }
        if (runs == 0.0) {
            continue;
        }
        mean_level /= runs;
        mean_length /= runs;

        double level_variance = 0.0;
        double length_variance = 0.0;
        double entropy = 0.0;
        for (int level = 0; level < gray_levels; ++level) {
            for (int length = 1; length <= matrix.max_length; ++length) {
                const int count = matrix.Count(level, length);
                if (count == 0) {
                    continue;
                }
                const double p = count / runs;
                level_variance += p * (level + 1 - mean_level) * (level + 1 - mean_level);
                length_variance += p * (length - mean_length) * (length - mean_length);
                entropy -= p * (log_base == LogBase::Two ? std::log2(p + EPSILON) : std::log(p + EPSILON));
            }
        }
        double level_uniformity = 0.0;
        for (double value : by_level) {
            level_uniformity += value * value;
        }
        double length_uniformity = 0.0;
        for (double value : by_length) {
            length_uniformity += value * value;
        }

        for (Type type : types) {
            double value = NAN_VALUE;
            switch (type) {
                case Type::GlrlmShortRunEmphasis:
                    value = short_runs / runs;
                    break;
                case Type::GlrlmLongRunEmphasis:
                    value = long_runs / runs;
                    break;
                case Type::GlrlmGrayLevelNonUniformity:
                    value = level_uniformity / runs;
                    break;
                case Type::GlrlmGrayLevelNonUniformityNormalized:
                    value = level_uniformity / (runs * runs);
                    break;
                case Type::GlrlmRunLengthNonUniformity:
                    value = length_uniformity / runs;
                    break;
                case Type::GlrlmRunLengthNonUniformityNormalized:
                    value = length_uniformity / (runs * runs);
                    break;
                case Type::GlrlmRunPercentage:
                    value = runs / pixels;
                    break;
                case Type::GlrlmGrayLevelVariance:
                    value = level_variance;
                    break;
                case Type::GlrlmRunVariance:
                    value = length_variance;
                    break;
                case Type::GlrlmRunEntropy:
                    value = entropy;
                    break;
                case Type::GlrlmLowGrayLevelRunEmphasis:
                    value = low_gray / runs;
                    break;
                case Type::GlrlmHighGrayLevelRunEmphasis:
                    value = high_gray / runs;
                    break;
                case Type::GlrlmShortRunLowGrayLevelEmphasis:
                    value = short_low / runs;
                    break;
                case Type::GlrlmShortRunHighGrayLevelEmphasis:
                    value = short_high / runs;
                    break;
                case Type::GlrlmLongRunLowGrayLevelEmphasis:
                    value = long_low / runs;
                    break;
                case Type::GlrlmLongRunHighGrayLevelEmphasis:
                    value = long_high / runs;
                    break;
                default:
                    break;
            }
            ValueOf(result[type], direction) = value;
        }
    }
    return result;
}

} // namespace glcm
