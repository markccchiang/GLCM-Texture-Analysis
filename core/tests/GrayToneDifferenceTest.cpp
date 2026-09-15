#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>
#include <set>
#include <stdexcept>
#include <vector>

#include "analysis/GrayToneDifference.hpp"
#include "imaging/ImageLoader.hpp"
#include "imaging/Quantizer.hpp"
#include "pipeline/FeatureCatalog.hpp"

using namespace glcm;

namespace {

const std::set<Type> ALL_GRAY_TONE{Type::NgtdmCoarseness, Type::NgtdmContrast, Type::NgtdmBusyness, Type::NgtdmComplexity, Type::NgtdmStrength};

// 0 0 1 1
// 0 0 1 2
// 2 2 2 2
cv::Mat Levels() {
    uchar data[] = {0, 0, 1, 1, 0, 0, 1, 2, 2, 2, 2, 2};
    return cv::Mat(3, 4, CV_8UC1, data).clone();
}

void ExpectClose(double actual, double expected) {
    EXPECT_NEAR(actual, expected, 1e-12 * std::max(1.0, std::abs(expected)));
}

} // namespace

TEST(GrayToneDifferenceTest, AveragesTheEightNeighboursAtDistanceOne) {
    const cv::Mat levels = Levels();
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    const GrayToneDifferenceMatrix matrix = ComputeGrayToneDifferenceMatrix(levels, mask, 3, 1);
    EXPECT_EQ(matrix.counts, (std::vector<int>{4, 3, 5}));
    // |i - neighbour average| per pixel, gray levels counted from 1, in row order:
    // gray level 1: 1 - 1, 1 - 7/5, 1 - 9/5, 1 - 16/8; gray level 2: 2 - 9/5, 2 - 7/3, 2 - 18/8;
    // gray level 3: 3 - 12/5, 3 - 5/3, 3 - 10/5, 3 - 12/5, 3 - 8/3
    ExpectClose(matrix.differences[0], 0.4 + 0.8 + 1.0);
    ExpectClose(matrix.differences[1], 0.2 + 1.0 / 3 + 0.25);
    ExpectClose(matrix.differences[2], 0.6 + 4.0 / 3 + 1.0 + 0.6 + 1.0 / 3);
}

TEST(GrayToneDifferenceTest, UsesOnlyTheRingAtTheDistance) {
    // 5 x 5: gray level 0 on the border, 2 around the centre, 1 in the centre. At distance 2 the centre's neighbours are
    // the 16 border pixels only (average 1), not the 8 pixels around it (a full square would average 40/24).
    cv::Mat levels(5, 5, CV_8UC1, cv::Scalar(0));
    levels(cv::Rect(1, 1, 3, 3)).setTo(2);
    levels.at<uchar>(2, 2) = 1;
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    const GrayToneDifferenceMatrix matrix = ComputeGrayToneDifferenceMatrix(levels, mask, 3, 2);
    EXPECT_EQ(matrix.counts[1], 1);
    EXPECT_EQ(matrix.differences[1], 1.0);
}

TEST(GrayToneDifferenceTest, CountsPixelsWithoutNeighboursWithoutADifference) {
    const cv::Mat levels = Levels();
    cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(0));
    mask.at<uchar>(0, 0) = 255;
    mask.at<uchar>(2, 3) = 255;
    const GrayToneDifferenceMatrix matrix = ComputeGrayToneDifferenceMatrix(levels, mask, 3, 1);
    EXPECT_EQ(matrix.counts, (std::vector<int>{1, 0, 1}));
    EXPECT_EQ(matrix.differences, (std::vector<double>{0.0, 0.0, 0.0}));
    // No difference at all: PyRadiomics' values for a homogeneous region
    const auto values = ComputeGrayToneDifferenceFeatures(levels, mask, 3, 1, ALL_GRAY_TONE);
    EXPECT_EQ(values.at(Type::NgtdmCoarseness), 1e6);
    EXPECT_EQ(values.at(Type::NgtdmBusyness), 0.0);
    EXPECT_EQ(values.at(Type::NgtdmStrength), 0.0);
}

TEST(GrayToneDifferenceTest, ComputesTheFeaturesFromTheMatrix) {
    const cv::Mat levels = Levels();
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    const auto values = ComputeGrayToneDifferenceFeatures(levels, mask, 3, 1, ALL_GRAY_TONE);

    // Gray levels 1, 2, 3 with p = 4/12, 3/12, 5/12 and s from the matrix above
    const std::array<double, 3> p{4.0 / 12, 3.0 / 12, 5.0 / 12};
    const std::array<double, 3> s{0.4 + 0.8 + 1.0, 0.2 + 1.0 / 3 + 0.25, 0.6 + 4.0 / 3 + 1.0 + 0.6 + 1.0 / 3};
    double ps = 0.0;
    double sum_s = 0.0;
    double contrast = 0.0;
    double busyness = 0.0;
    double complexity = 0.0;
    double strength = 0.0;
    for (int a = 0; a < 3; ++a) {
        ps += p[a] * s[a];
        sum_s += s[a];
        for (int b = 0; b < 3; ++b) {
            const double i = a + 1;
            const double j = b + 1;
            contrast += p[a] * p[b] * (i - j) * (i - j);
            busyness += std::abs(i * p[a] - j * p[b]);
            complexity += std::abs(i - j) * (p[a] * s[a] + p[b] * s[b]) / (p[a] + p[b]);
            strength += (p[a] + p[b]) * (i - j) * (i - j);
        }
    }
    ExpectClose(values.at(Type::NgtdmCoarseness), 1.0 / ps);
    ExpectClose(values.at(Type::NgtdmContrast), contrast / (3 * 2) * sum_s / 12);
    ExpectClose(values.at(Type::NgtdmBusyness), ps / busyness);
    ExpectClose(values.at(Type::NgtdmComplexity), complexity / 12);
    ExpectClose(values.at(Type::NgtdmStrength), strength / sum_s);

    const cv::Mat empty(levels.size(), CV_8UC1, cv::Scalar(0));
    EXPECT_TRUE(std::isnan(ComputeGrayToneDifferenceFeatures(levels, empty, 3, 1, {Type::NgtdmStrength}).at(Type::NgtdmStrength)));
}

TEST(GrayToneDifferenceTest, RejectsOtherFeaturesAndInvalidInput) {
    const cv::Mat levels = Levels();
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    EXPECT_TRUE(IsGrayToneDifferenceFeature(Type::NgtdmBusyness));
    EXPECT_FALSE(IsGrayToneDifferenceFeature(Type::Contrast));
    EXPECT_THROW(ComputeGrayToneDifferenceFeatures(levels, mask, 3, 1, {Type::Contrast}), std::invalid_argument);
    EXPECT_THROW(ComputeGrayToneDifferenceMatrix(levels, mask, 2, 1), std::invalid_argument); // level 2 with 2 levels
    EXPECT_THROW(ComputeGrayToneDifferenceMatrix(levels, mask, 3, 0), std::invalid_argument);
    EXPECT_THROW(ComputeGrayToneDifferenceMatrix(levels, mask(cv::Rect(0, 0, 3, 3)), 3, 1), std::invalid_argument);
}

TEST(GrayToneDifferenceTest, MatchesPyRadiomicsOnSampleImages) {
    // Written by scripts/radiomics-reference.py: PyRadiomics NGTDM with a bin width of 1 in 2D, at distances 1 and 2
    std::ifstream stream(GLCM_SOURCE_DIR "/core/tests/data/pyradiomics-ngtdm.json");
    ASSERT_TRUE(stream) << "missing core/tests/data/pyradiomics-ngtdm.json";
    const nlohmann::json document = nlohmann::json::parse(stream);
    ASSERT_GE(document.at("cases").size(), 4u);

    for (const nlohmann::json& reference : document.at("cases")) {
        const std::string image_path = reference.at("image");
        const int distance = reference.at("distance");
        SCOPED_TRACE(image_path + " at distance " + std::to_string(distance));
        const LoadedImage image = LoadImageFile(GLCM_SOURCE_DIR "/samples/" + image_path);
        const auto& rectangle = reference.at("rectangle");
        const cv::Rect box(rectangle.at(0), rectangle.at(1), rectangle.at(2), rectangle.at(3));
        const cv::Mat mask(box.size(), CV_8UC1, cv::Scalar(255));
        // A fixed bin width of 1 starts at the ROI minimum, like PyRadiomics' bins
        QuantizationSettings quantization;
        quantization.method = QuantizationMethod::FixedBinWidth;
        quantization.bin_width = 1.0;
        const QuantizationResult quantized = Quantize(image.gray(box), mask, 256, quantization);

        const auto values = ComputeGrayToneDifferenceFeatures(quantized.image, mask, 256, distance, ALL_GRAY_TONE);
        for (const auto& [name, value] : reference.at("features").items()) {
            const double expected = value.get<double>();
            const std::optional<Type> type = FeatureTypeFromId("Ngtdm" + name);
            ASSERT_TRUE(type.has_value()) << name;
            EXPECT_NEAR(values.at(*type), expected, 1e-9 * std::max(1.0, std::abs(expected))) << name;
        }
    }
}
