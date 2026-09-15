#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>
#include <set>
#include <stdexcept>
#include <utility>

#include "analysis/SizeZone.hpp"
#include "imaging/ImageLoader.hpp"
#include "imaging/Quantizer.hpp"
#include "pipeline/FeatureCatalog.hpp"

using namespace glcm;

namespace {

using ZoneMap = std::map<std::pair<int, int>, int>; // (gray level, size) -> number of zones

const std::set<Type> ALL_SIZE_ZONE{Type::GlszmSmallAreaEmphasis, Type::GlszmLargeAreaEmphasis, Type::GlszmGrayLevelNonUniformity, Type::GlszmGrayLevelNonUniformityNormalized, Type::GlszmSizeZoneNonUniformity, Type::GlszmSizeZoneNonUniformityNormalized, Type::GlszmZonePercentage, Type::GlszmGrayLevelVariance, Type::GlszmZoneVariance, Type::GlszmZoneEntropy, Type::GlszmLowGrayLevelZoneEmphasis, Type::GlszmHighGrayLevelZoneEmphasis, Type::GlszmSmallAreaLowGrayLevelEmphasis, Type::GlszmSmallAreaHighGrayLevelEmphasis, Type::GlszmLargeAreaLowGrayLevelEmphasis, Type::GlszmLargeAreaHighGrayLevelEmphasis};
const double EPSILON = std::numeric_limits<double>::epsilon();

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

TEST(SizeZoneTest, CountsEightConnectedZones) {
    const cv::Mat levels = Levels();
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    // The 2 at the end of the second row touches the bottom row diagonally and directly
    EXPECT_EQ(ComputeSizeZoneMatrix(levels, mask, 3).counts, (ZoneMap{{{0, 4}, 1}, {{1, 3}, 1}, {{2, 5}, 1}}));

    // 1 0 0
    // 0 1 0
    // 0 0 1
    // Diagonal neighbours connect: one zone of 1s, and the 0s on both sides meet at the corners of the diagonal
    uchar diagonal_data[] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    const cv::Mat diagonal = cv::Mat(3, 3, CV_8UC1, diagonal_data).clone();
    const cv::Mat full(diagonal.size(), CV_8UC1, cv::Scalar(255));
    EXPECT_EQ(ComputeSizeZoneMatrix(diagonal, full, 2).counts, (ZoneMap{{{0, 6}, 1}, {{1, 3}, 1}}));
}

TEST(SizeZoneTest, PixelsOutsideTheMaskSplitZones) {
    const cv::Mat levels = Levels();
    cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    mask.at<uchar>(2, 1) = 0;
    mask.at<uchar>(2, 2) = 0;
    EXPECT_EQ(ComputeSizeZoneMatrix(levels, mask, 3).counts, (ZoneMap{{{0, 4}, 1}, {{1, 3}, 1}, {{2, 1}, 1}, {{2, 2}, 1}}));
}

TEST(SizeZoneTest, ComputesTheFeaturesFromTheZones) {
    const cv::Mat levels = Levels();
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    const auto values = ComputeSizeZoneFeatures(levels, mask, 3, LogBase::Two, ALL_SIZE_ZONE);
    ASSERT_EQ(values.size(), ALL_SIZE_ZONE.size());

    // Zones (i = level + 1, size j): (1, 4), (2, 3), (3, 5); 3 zones over 12 pixels
    ExpectClose(values.at(Type::GlszmSmallAreaEmphasis), (1.0 / 16 + 1.0 / 9 + 1.0 / 25) / 3);
    ExpectClose(values.at(Type::GlszmLargeAreaEmphasis), (16.0 + 9 + 25) / 3);
    ExpectClose(values.at(Type::GlszmGrayLevelNonUniformity), 3.0 / 3);
    ExpectClose(values.at(Type::GlszmGrayLevelNonUniformityNormalized), 3.0 / 9);
    ExpectClose(values.at(Type::GlszmSizeZoneNonUniformity), 3.0 / 3);
    ExpectClose(values.at(Type::GlszmSizeZoneNonUniformityNormalized), 3.0 / 9);
    ExpectClose(values.at(Type::GlszmZonePercentage), 3.0 / 12);
    ExpectClose(values.at(Type::GlszmGrayLevelVariance), 2.0 / 3);
    ExpectClose(values.at(Type::GlszmZoneVariance), 2.0 / 3);
    ExpectClose(values.at(Type::GlszmZoneEntropy), -3 * (1.0 / 3) * std::log2(1.0 / 3 + EPSILON));
    ExpectClose(values.at(Type::GlszmLowGrayLevelZoneEmphasis), (1.0 + 1.0 / 4 + 1.0 / 9) / 3);
    ExpectClose(values.at(Type::GlszmHighGrayLevelZoneEmphasis), (1.0 + 4 + 9) / 3);
    ExpectClose(values.at(Type::GlszmSmallAreaLowGrayLevelEmphasis), (1.0 / 16 + 1.0 / (4 * 9) + 1.0 / (9 * 25)) / 3);
    ExpectClose(values.at(Type::GlszmSmallAreaHighGrayLevelEmphasis), (1.0 / 16 + 4.0 / 9 + 9.0 / 25) / 3);
    ExpectClose(values.at(Type::GlszmLargeAreaLowGrayLevelEmphasis), (16.0 + 9.0 / 4 + 25.0 / 9) / 3);
    ExpectClose(values.at(Type::GlszmLargeAreaHighGrayLevelEmphasis), (16.0 + 4.0 * 9 + 9.0 * 25) / 3);

    const cv::Mat empty(levels.size(), CV_8UC1, cv::Scalar(0));
    EXPECT_TRUE(std::isnan(ComputeSizeZoneFeatures(levels, empty, 3, LogBase::Two, {Type::GlszmZoneEntropy}).at(Type::GlszmZoneEntropy)));
}

TEST(SizeZoneTest, RejectsOtherFeaturesAndInvalidInput) {
    const cv::Mat levels = Levels();
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    EXPECT_TRUE(IsSizeZoneFeature(Type::GlszmZoneEntropy));
    EXPECT_FALSE(IsSizeZoneFeature(Type::GlrlmRunEntropy));
    EXPECT_THROW(ComputeSizeZoneFeatures(levels, mask, 3, LogBase::Two, {Type::Contrast}), std::invalid_argument);
    EXPECT_THROW(ComputeSizeZoneMatrix(levels, mask, 2), std::invalid_argument); // level 2 with 2 levels
    EXPECT_THROW(ComputeSizeZoneMatrix(levels, mask(cv::Rect(0, 0, 3, 3)), 3), std::invalid_argument);
}

TEST(SizeZoneTest, MatchesPyRadiomicsOnSampleImages) {
    // Written by scripts/radiomics-reference.py: PyRadiomics GLSZM with a bin width of 1 in 2D
    std::ifstream stream(GLCM_SOURCE_DIR "/core/tests/data/pyradiomics-glszm.json");
    ASSERT_TRUE(stream) << "missing core/tests/data/pyradiomics-glszm.json";
    const nlohmann::json document = nlohmann::json::parse(stream);
    ASSERT_GE(document.at("cases").size(), 2u);

    for (const nlohmann::json& reference : document.at("cases")) {
        const std::string image_path = reference.at("image");
        SCOPED_TRACE(image_path);
        const LoadedImage image = LoadImageFile(GLCM_SOURCE_DIR "/samples/" + image_path);
        const auto& rectangle = reference.at("rectangle");
        const cv::Rect box(rectangle.at(0), rectangle.at(1), rectangle.at(2), rectangle.at(3));
        const cv::Mat mask(box.size(), CV_8UC1, cv::Scalar(255));
        // A fixed bin width of 1 starts at the ROI minimum, like PyRadiomics' bins
        QuantizationSettings quantization;
        quantization.method = QuantizationMethod::FixedBinWidth;
        quantization.bin_width = 1.0;
        const QuantizationResult quantized = Quantize(image.gray(box), mask, 256, quantization);

        const auto values = ComputeSizeZoneFeatures(quantized.image, mask, 256, LogBase::Two, ALL_SIZE_ZONE);
        for (const auto& [name, value] : reference.at("features").items()) {
            const double expected = value.get<double>();
            const std::optional<Type> type = FeatureTypeFromId("Glszm" + name);
            ASSERT_TRUE(type.has_value()) << name;
            EXPECT_NEAR(values.at(*type), expected, 1e-9 * std::max(1.0, std::abs(expected))) << name;
        }
    }
}
