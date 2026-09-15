#include "analysis/LocalBinaryPattern.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace glcm {

namespace {

const double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
const uchar INSIDE = 255;
const double EPSILON = std::numeric_limits<double>::epsilon();
// numpy.pi
const double PI = 3.141592653589793;

struct SampleOffsets {
    std::array<double, LBP_SAMPLES> row{};
    std::array<double, LBP_SAMPLES> col{};
};

// scikit-image: rr = -R sin(2 pi k / P) and cc = R cos(2 pi k / P), rounded to 5 decimals like np.round (scaled, rounded
// half to even, scaled back). The rounding also hides the last-bit differences between sine implementations.
SampleOffsets OffsetsFor(int radius) {
    SampleOffsets offsets;
    const double r = radius;
    for (int k = 0; k < LBP_SAMPLES; ++k) {
        const double angle = 2 * PI * k / LBP_SAMPLES;
        offsets.row[static_cast<size_t>(k)] = std::nearbyint(-r * std::sin(angle) * 100000.0) / 100000.0;
        offsets.col[static_cast<size_t>(k)] = std::nearbyint(r * std::cos(angle) * 100000.0) / 100000.0;
    }
    return offsets;
}

void CheckImage(const cv::Mat& gray, int radius) {
    if (gray.empty() || gray.channels() != 1 || (gray.depth() != CV_8U && gray.depth() != CV_16U)) {
        throw std::invalid_argument("The image must be a non-empty 8- or 16-bit single-channel image");
    }
    if (radius < 1) {
        throw std::invalid_argument("The LBP radius must be at least 1");
    }
}

// Constant mode of scikit-image's get_pixel2d: 0 outside the image
double PixelAt(const cv::Mat& gray, long row, long col) {
    if (row < 0 || row >= gray.rows || col < 0 || col >= gray.cols) {
        return 0.0;
    }
    return gray.depth() == CV_16U ? gray.at<uint16_t>(static_cast<int>(row), static_cast<int>(col))
                                  : gray.at<uchar>(static_cast<int>(row), static_cast<int>(col));
}

// scikit-image's bilinear_interpolation, with the same order of operations so that ties with the centre agree
double Interpolate(const cv::Mat& gray, double row, double col) {
    const auto min_row = static_cast<long>(std::floor(row));
    const auto min_col = static_cast<long>(std::floor(col));
    const auto max_row = static_cast<long>(std::ceil(row));
    const auto max_col = static_cast<long>(std::ceil(col));
    const double dr = row - static_cast<double>(min_row);
    const double dc = col - static_cast<double>(min_col);
    const double top = (1 - dc) * PixelAt(gray, min_row, min_col) + dc * PixelAt(gray, min_row, max_col);
    const double bottom = (1 - dc) * PixelAt(gray, max_row, min_col) + dc * PixelAt(gray, max_row, max_col);
    return (1 - dr) * top + dr * bottom;
}

int CodeAt(const cv::Mat& gray, int row, int col, const SampleOffsets& offsets) {
    const double centre = PixelAt(gray, row, col);
    std::array<int, LBP_SAMPLES> set{};
    for (size_t k = 0; k < LBP_SAMPLES; ++k) {
        const double sample = Interpolate(gray, row + offsets.row[k], col + offsets.col[k]);
        set[k] = sample - centre >= 0 ? 1 : 0;
    }
    int changes = 0;
    for (size_t k = 0; k + 1 < LBP_SAMPLES; ++k) {
        changes += set[k] != set[k + 1] ? 1 : 0;
    }
    if (changes > 2) {
        return LBP_SAMPLES + 1;
    }
    int ones = 0;
    for (int value : set) {
        ones += value;
    }
    return ones;
}

// Index in the histogram of a code fraction feature, or -1
int CodeIndex(Type type) {
    switch (type) {
        case Type::LbpUniform0:
            return 0;
        case Type::LbpUniform1:
            return 1;
        case Type::LbpUniform2:
            return 2;
        case Type::LbpUniform3:
            return 3;
        case Type::LbpUniform4:
            return 4;
        case Type::LbpUniform5:
            return 5;
        case Type::LbpUniform6:
            return 6;
        case Type::LbpUniform7:
            return 7;
        case Type::LbpUniform8:
            return 8;
        case Type::LbpNonUniform:
            return 9;
        default:
            return -1;
    }
}

} // namespace

bool IsLocalBinaryPatternFeature(Type type) {
    switch (type) {
        case Type::LbpUniform0:
        case Type::LbpUniform1:
        case Type::LbpUniform2:
        case Type::LbpUniform3:
        case Type::LbpUniform4:
        case Type::LbpUniform5:
        case Type::LbpUniform6:
        case Type::LbpUniform7:
        case Type::LbpUniform8:
        case Type::LbpNonUniform:
        case Type::LbpEntropy:
        case Type::LbpEnergy:
            return true;
        default:
            return false;
    }
}

int LocalBinaryPatternCode(const cv::Mat& gray, int row, int col, int radius) {
    CheckImage(gray, radius);
    if (row < 0 || row >= gray.rows || col < 0 || col >= gray.cols) {
        throw std::invalid_argument("The pixel is outside the image");
    }
    return CodeAt(gray, row, col, OffsetsFor(radius));
}

std::array<int, LBP_CODES> ComputeLocalBinaryPatternHistogram(const cv::Mat& gray, const cv::Rect& box, const cv::Mat& mask, int radius) {
    CheckImage(gray, radius);
    if (box.x < 0 || box.y < 0 || box.width < 0 || box.height < 0 || box.x + box.width > gray.cols || box.y + box.height > gray.rows) {
        throw std::invalid_argument("The box must lie inside the image");
    }
    if (mask.type() != CV_8UC1 || mask.cols != box.width || mask.rows != box.height) {
        throw std::invalid_argument("The mask must be an 8-bit single-channel image of the size of the box");
    }
    const SampleOffsets offsets = OffsetsFor(radius);
    std::array<int, LBP_CODES> counts{};
    for (int row = 0; row < mask.rows; ++row) {
        const uchar* line = mask.ptr<uchar>(row);
        for (int col = 0; col < mask.cols; ++col) {
            if (line[col] == INSIDE) {
                ++counts[static_cast<size_t>(CodeAt(gray, box.y + row, box.x + col, offsets))];
            }
        }
    }
    return counts;
}

std::map<Type, double> ComputeLocalBinaryPatternFeatures(const cv::Mat& gray, const cv::Rect& box, const cv::Mat& mask, int radius,
    LogBase log_base, const std::set<Type>& types) {
    for (Type type : types) {
        if (!IsLocalBinaryPatternFeature(type)) {
            throw std::invalid_argument(TextureAnalysis::TypeToString(type) + " is not computed by ComputeLocalBinaryPatternFeatures");
        }
    }
    const std::array<int, LBP_CODES> counts = ComputeLocalBinaryPatternHistogram(gray, box, mask, radius);
    std::map<Type, double> result;
    double pixels = 0.0;
    for (int count : counts) {
        pixels += count;
    }
    if (pixels == 0.0) {
        for (Type type : types) {
            result[type] = NAN_VALUE;
        }
        return result;
    }

    std::array<double, LBP_CODES> fractions{};
    double entropy = 0.0;
    double energy = 0.0;
    for (size_t k = 0; k < LBP_CODES; ++k) {
        const double fraction = counts[k] / pixels;
        fractions[k] = fraction;
        if (fraction > 0) {
            entropy -= fraction * (log_base == LogBase::Two ? std::log2(fraction + EPSILON) : std::log(fraction + EPSILON));
        }
        energy += fraction * fraction;
    }
    for (Type type : types) {
        if (type == Type::LbpEntropy) {
            result[type] = entropy;
        } else if (type == Type::LbpEnergy) {
            result[type] = energy;
        } else {
            result[type] = fractions[static_cast<size_t>(CodeIndex(type))];
        }
    }
    return result;
}

} // namespace glcm
