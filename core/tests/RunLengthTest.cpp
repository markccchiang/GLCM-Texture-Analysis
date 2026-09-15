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

#include "analysis/RunLength.hpp"
#include "imaging/ImageLoader.hpp"
#include "imaging/Quantizer.hpp"
#include "pipeline/FeatureCatalog.hpp"

using namespace glcm;

namespace {

using RunMap = std::map<std::pair<int, int>, int>; // (gray level, length) -> number of runs

const std::set<Type> ALL_RUN_LENGTH{Type::GlrlmShortRunEmphasis, Type::GlrlmLongRunEmphasis, Type::GlrlmGrayLevelNonUniformity, Type::GlrlmGrayLevelNonUniformityNormalized, Type::GlrlmRunLengthNonUniformity, Type::GlrlmRunLengthNonUniformityNormalized, Type::GlrlmRunPercentage, Type::GlrlmGrayLevelVariance, Type::GlrlmRunVariance, Type::GlrlmRunEntropy, Type::GlrlmLowGrayLevelRunEmphasis, Type::GlrlmHighGrayLevelRunEmphasis, Type::GlrlmShortRunLowGrayLevelEmphasis, Type::GlrlmShortRunHighGrayLevelEmphasis, Type::GlrlmLongRunLowGrayLevelEmphasis, Type::GlrlmLongRunHighGrayLevelEmphasis};
const std::set<Direction> ALL_DIRECTIONS{Direction::H, Direction::V, Direction::LD, Direction::RD};
const double EPSILON = std::numeric_limits<double>::epsilon();

cv::Mat Levels() {
    uchar data[] = {0, 0, 1, 1, 0, 0, 1, 2, 2, 2, 2, 2};
    return cv::Mat(3, 4, CV_8UC1, data).clone();
}

RunMap Runs(const RunLengthMatrix& matrix) {
    RunMap runs;
    for (int level = 0; level < matrix.gray_levels; ++level) {
        for (int length = 1; length <= matrix.max_length; ++length) {
            if (matrix.Count(level, length) > 0) {
                runs[{level, length}] = matrix.Count(level, length);
            }
        }
    }
    return runs;
}

void ExpectClose(double actual, double expected) {
    EXPECT_NEAR(actual, expected, 1e-12 * std::max(1.0, std::abs(expected)));
}

} // namespace

TEST(RunLengthTest, CountsRunsInEveryDirection) {
    // 0 0 1 1
    // 0 0 1 2
    // 2 2 2 2
    const cv::Mat levels = Levels();
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    EXPECT_EQ(Runs(ComputeRunLengthMatrix(levels, mask, 3, Direction::H)), (RunMap{{{0, 2}, 2}, {{1, 1}, 1}, {{1, 2}, 1}, {{2, 1}, 1}, {{2, 4}, 1}}));
    EXPECT_EQ(Runs(ComputeRunLengthMatrix(levels, mask, 3, Direction::V)), (RunMap{{{0, 2}, 2}, {{1, 1}, 1}, {{1, 2}, 1}, {{2, 1}, 3}, {{2, 2}, 1}}));
    // 135°: down and to the right
    EXPECT_EQ(Runs(ComputeRunLengthMatrix(levels, mask, 3, Direction::LD)), (RunMap{{{0, 1}, 2}, {{0, 2}, 1}, {{1, 1}, 3}, {{2, 1}, 5}}));
    // 45°: down and to the left
    EXPECT_EQ(Runs(ComputeRunLengthMatrix(levels, mask, 3, Direction::RD)),
        (RunMap{{{0, 1}, 2}, {{0, 2}, 1}, {{1, 1}, 1}, {{1, 2}, 1}, {{2, 1}, 3}, {{2, 2}, 1}}));
}

TEST(RunLengthTest, PixelsOutsideTheMaskEndRuns) {
    const cv::Mat levels = Levels();
    cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    mask.at<uchar>(2, 1) = 0;
    EXPECT_EQ(Runs(ComputeRunLengthMatrix(levels, mask, 3, Direction::H)), (RunMap{{{0, 2}, 2}, {{1, 1}, 1}, {{1, 2}, 1}, {{2, 1}, 2}, {{2, 2}, 1}}));
}

TEST(RunLengthTest, ComputesTheFeaturesFromTheRuns) {
    const cv::Mat levels = Levels();
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    const auto values = ComputeRunLengthFeatures(levels, mask, 3, {Direction::H}, LogBase::Two, ALL_RUN_LENGTH);
    const auto h = [&](Type type) { return values.at(type).H; };

    // 0°: runs (i = level + 1, j = length): (1, 2) twice, (2, 2), (2, 1), (3, 1), (3, 4); 6 runs over 12 pixels
    ExpectClose(h(Type::GlrlmShortRunEmphasis), (2.0 / 4 + 1.0 / 4 + 1.0 + 1.0 + 1.0 / 16) / 6);
    ExpectClose(h(Type::GlrlmLongRunEmphasis), (2.0 * 4 + 4 + 1 + 1 + 16) / 6);
    ExpectClose(h(Type::GlrlmGrayLevelNonUniformity), (4.0 + 4 + 4) / 6);
    ExpectClose(h(Type::GlrlmGrayLevelNonUniformityNormalized), (4.0 + 4 + 4) / 36);
    ExpectClose(h(Type::GlrlmRunLengthNonUniformity), (4.0 + 9 + 1) / 6);
    ExpectClose(h(Type::GlrlmRunLengthNonUniformityNormalized), (4.0 + 9 + 1) / 36);
    ExpectClose(h(Type::GlrlmRunPercentage), 6.0 / 12);
    ExpectClose(h(Type::GlrlmGrayLevelVariance), 2.0 / 3);
    ExpectClose(h(Type::GlrlmRunVariance), 1.0);
    ExpectClose(h(Type::GlrlmRunEntropy), -(2.0 / 6 * std::log2(2.0 / 6 + EPSILON) + 4 * (1.0 / 6) * std::log2(1.0 / 6 + EPSILON)));
    ExpectClose(h(Type::GlrlmLowGrayLevelRunEmphasis), (2.0 + 1.0 / 4 + 1.0 / 4 + 1.0 / 9 + 1.0 / 9) / 6);
    ExpectClose(h(Type::GlrlmHighGrayLevelRunEmphasis), (2.0 + 4 + 4 + 9 + 9) / 6);
    ExpectClose(h(Type::GlrlmShortRunLowGrayLevelEmphasis), (2.0 / 4 + 1.0 / 16 + 1.0 / 4 + 1.0 / 9 + 1.0 / (9 * 16)) / 6);
    ExpectClose(h(Type::GlrlmShortRunHighGrayLevelEmphasis), (2.0 / 4 + 4.0 / 4 + 4.0 + 9.0 + 9.0 / 16) / 6);
    ExpectClose(h(Type::GlrlmLongRunLowGrayLevelEmphasis), (2.0 * 4 + 4.0 / 4 + 1.0 / 4 + 1.0 / 9 + 16.0 / 9) / 6);
    ExpectClose(h(Type::GlrlmLongRunHighGrayLevelEmphasis), (2.0 * 4 + 4.0 * 4 + 4.0 + 9.0 + 9.0 * 16) / 6);

    // Directions that were not requested are NaN
    EXPECT_TRUE(std::isnan(values.at(Type::GlrlmRunPercentage).V));
    const cv::Mat empty(levels.size(), CV_8UC1, cv::Scalar(0));
    EXPECT_TRUE(std::isnan(ComputeRunLengthFeatures(levels, empty, 3, ALL_DIRECTIONS, LogBase::Two, {Type::GlrlmRunEntropy}).at(Type::GlrlmRunEntropy).Avg()));
}

TEST(RunLengthTest, RejectsOtherFeaturesAndInvalidInput) {
    const cv::Mat levels = Levels();
    const cv::Mat mask(levels.size(), CV_8UC1, cv::Scalar(255));
    EXPECT_TRUE(IsRunLengthFeature(Type::GlrlmRunEntropy));
    EXPECT_FALSE(IsRunLengthFeature(Type::Entropy));
    EXPECT_THROW(ComputeRunLengthFeatures(levels, mask, 3, ALL_DIRECTIONS, LogBase::Two, {Type::Contrast}), std::invalid_argument);
    EXPECT_THROW(ComputeRunLengthMatrix(levels, mask, 2, Direction::H), std::invalid_argument); // level 2 with 2 levels
    EXPECT_THROW(ComputeRunLengthMatrix(levels, mask, 3, Direction::Avg), std::invalid_argument);
    EXPECT_THROW(ComputeRunLengthMatrix(levels, mask(cv::Rect(0, 0, 3, 3)), 3, Direction::H), std::invalid_argument);
}

TEST(RunLengthTest, MatchesPyRadiomicsOnSampleImages) {
    // Written by scripts/radiomics-reference.py: PyRadiomics GLRLM with a bin width of 1 in 2D (four directions, averaged)
    std::ifstream stream(GLCM_SOURCE_DIR "/core/tests/data/pyradiomics-glrlm.json");
    ASSERT_TRUE(stream) << "missing core/tests/data/pyradiomics-glrlm.json";
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

        const auto values = ComputeRunLengthFeatures(quantized.image, mask, 256, ALL_DIRECTIONS, LogBase::Two, ALL_RUN_LENGTH);
        for (const auto& [name, value] : reference.at("features").items()) {
            const double expected = value.get<double>();
            const std::optional<Type> type = FeatureTypeFromId("Glrlm" + name);
            ASSERT_TRUE(type.has_value()) << name;
            EXPECT_NEAR(values.at(*type).Avg(), expected, 1e-9 * std::max(1.0, std::abs(expected))) << name;
        }
    }
}
