#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>
#include <set>
#include <stdexcept>

#include "analysis/LocalBinaryPattern.hpp"
#include "imaging/ImageLoader.hpp"
#include "pipeline/FeatureCatalog.hpp"

using namespace glcm;

namespace {

const std::set<Type> ALL_LBP{Type::LbpUniform0, Type::LbpUniform1, Type::LbpUniform2, Type::LbpUniform3, Type::LbpUniform4, Type::LbpUniform5, Type::LbpUniform6, Type::LbpUniform7, Type::LbpUniform8, Type::LbpNonUniform, Type::LbpEntropy, Type::LbpEnergy};
const double EPSILON = std::numeric_limits<double>::epsilon();

cv::Mat Image3x3(const std::array<uchar, 9>& values) {
    cv::Mat image(3, 3, CV_8UC1);
    for (int i = 0; i < 9; ++i) {
        image.at<uchar>(i / 3, i % 3) = values[static_cast<size_t>(i)];
    }
    return image;
}

void ExpectClose(double actual, double expected) {
    EXPECT_NEAR(actual, expected, 1e-12 * std::max(1.0, std::abs(expected)));
}

} // namespace

TEST(LocalBinaryPatternTest, CodesUniformAndNonUniformPatterns) {
    // Samples counter-clockwise from the right. Right, upper right, up and upper left are brighter than the centre:
    // one run of four set samples, a uniform pattern with code 4. (The upper-right sample interpolates to about 8.66.)
    EXPECT_EQ(LocalBinaryPatternCode(Image3x3({9, 9, 9, 0, 5, 9, 0, 0, 0}), 1, 1, 1), 4);
    // Direct neighbours 9, diagonal samples about 4.16: alternating samples, a non-uniform pattern
    EXPECT_EQ(LocalBinaryPatternCode(Image3x3({0, 9, 0, 9, 5, 9, 0, 9, 0}), 1, 1, 1), LBP_SAMPLES + 1);
    // All samples darker: code 0 (a local maximum)
    EXPECT_EQ(LocalBinaryPatternCode(Image3x3({1, 1, 1, 1, 5, 1, 1, 1, 1}), 1, 1, 1), 0);
}

TEST(LocalBinaryPatternTest, SamplesOutsideTheImageAreZero) {
    EXPECT_EQ(LocalBinaryPatternCode(cv::Mat(1, 1, CV_8UC1, cv::Scalar(5)), 0, 0, 1), 0);
    // A sample equal to the centre is set
    EXPECT_EQ(LocalBinaryPatternCode(cv::Mat(1, 1, CV_16UC1, cv::Scalar(0)), 0, 0, 1), LBP_SAMPLES);
}

TEST(LocalBinaryPatternTest, FeaturesAreTheHistogramOfTheRoiCodes) {
    cv::Mat gray(6, 7, CV_16UC1);
    for (int row = 0; row < gray.rows; ++row) {
        for (int col = 0; col < gray.cols; ++col) {
            gray.at<uint16_t>(row, col) = static_cast<uint16_t>((row * 7 + col * 3) % 11 * 1000);
        }
    }
    const cv::Rect box(1, 2, 4, 3);
    cv::Mat mask(box.size(), CV_8UC1, cv::Scalar(255));
    mask.at<uchar>(1, 2) = 0;

    // The codes of the ROI's pixels, sampled in the whole image
    std::array<int, LBP_CODES> expected{};
    for (int row = 0; row < box.height; ++row) {
        for (int col = 0; col < box.width; ++col) {
            if (mask.at<uchar>(row, col) == 255) {
                ++expected[static_cast<size_t>(LocalBinaryPatternCode(gray, box.y + row, box.x + col, 2))];
            }
        }
    }
    EXPECT_EQ(ComputeLocalBinaryPatternHistogram(gray, box, mask, 2), expected);

    const auto values = ComputeLocalBinaryPatternFeatures(gray, box, mask, 2, LogBase::Two, ALL_LBP);
    ASSERT_EQ(values.size(), ALL_LBP.size());
    double entropy = 0.0;
    double energy = 0.0;
    for (int k = 0; k < LBP_CODES; ++k) {
        const double fraction = expected[static_cast<size_t>(k)] / 11.0;
        if (fraction > 0) {
            entropy -= fraction * std::log2(fraction + EPSILON);
        }
        energy += fraction * fraction;
        const Type type = k < LBP_SAMPLES + 1 ? *FeatureTypeFromId("LbpUniform" + std::to_string(k)) : Type::LbpNonUniform;
        ExpectClose(values.at(type), fraction);
    }
    ExpectClose(values.at(Type::LbpEntropy), entropy);
    ExpectClose(values.at(Type::LbpEnergy), energy);

    const cv::Mat empty(box.size(), CV_8UC1, cv::Scalar(0));
    EXPECT_TRUE(std::isnan(ComputeLocalBinaryPatternFeatures(gray, box, empty, 1, LogBase::Two, {Type::LbpEnergy}).at(Type::LbpEnergy)));
}

TEST(LocalBinaryPatternTest, RejectsOtherFeaturesAndInvalidInput) {
    const cv::Mat gray(6, 7, CV_8UC1, cv::Scalar(3));
    const cv::Mat mask(3, 4, CV_8UC1, cv::Scalar(255));
    EXPECT_TRUE(IsLocalBinaryPatternFeature(Type::LbpUniform3));
    EXPECT_FALSE(IsLocalBinaryPatternFeature(Type::NgtdmStrength));
    EXPECT_THROW(ComputeLocalBinaryPatternFeatures(gray, cv::Rect(0, 0, 4, 3), mask, 1, LogBase::Two, {Type::Contrast}), std::invalid_argument);
    EXPECT_THROW(ComputeLocalBinaryPatternHistogram(gray, cv::Rect(0, 0, 4, 3), mask, 0), std::invalid_argument);
    EXPECT_THROW(ComputeLocalBinaryPatternHistogram(gray, cv::Rect(4, 4, 4, 3), mask, 1), std::invalid_argument);
    EXPECT_THROW(ComputeLocalBinaryPatternHistogram(gray, cv::Rect(0, 0, 3, 3), mask, 1), std::invalid_argument);
    EXPECT_THROW(LocalBinaryPatternCode(gray, 6, 0, 1), std::invalid_argument);
    EXPECT_THROW(LocalBinaryPatternCode(cv::Mat(3, 3, CV_32FC1), 0, 0, 1), std::invalid_argument);
}

TEST(LocalBinaryPatternTest, MatchesScikitImageOnSampleImages) {
    // Written by scripts/radiomics-reference.py: scikit-image's local_binary_pattern(image, 8, radius, 'uniform') on
    // the whole image, restricted to the rectangle
    std::ifstream stream(GLCM_SOURCE_DIR "/core/tests/data/scikit-image-lbp.json");
    ASSERT_TRUE(stream) << "missing core/tests/data/scikit-image-lbp.json";
    const nlohmann::json document = nlohmann::json::parse(stream);
    ASSERT_GE(document.at("cases").size(), 6u);

    for (const nlohmann::json& reference : document.at("cases")) {
        const std::string image_path = reference.at("image");
        const int radius = reference.at("radius");
        SCOPED_TRACE(image_path + " at radius " + std::to_string(radius));
        const LoadedImage image = LoadImageFile(GLCM_SOURCE_DIR "/samples/" + image_path);
        const auto& rectangle = reference.at("rectangle");
        const cv::Rect box(rectangle.at(0), rectangle.at(1), rectangle.at(2), rectangle.at(3));
        const cv::Mat mask(box.size(), CV_8UC1, cv::Scalar(255));

        const auto values = ComputeLocalBinaryPatternFeatures(image.gray, box, mask, radius, LogBase::Two, ALL_LBP);
        for (const auto& [name, value] : reference.at("features").items()) {
            const double expected = value.get<double>();
            const std::optional<Type> type = FeatureTypeFromId("Lbp" + name);
            ASSERT_TRUE(type.has_value()) << name;
            EXPECT_NEAR(values.at(*type), expected, 1e-12 * std::max(1.0, std::abs(expected))) << name;
        }
    }
}
