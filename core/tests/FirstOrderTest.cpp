#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>
#include <set>
#include <stdexcept>
#include <vector>

#include "analysis/FirstOrder.hpp"
#include "imaging/ImageLoader.hpp"
#include "pipeline/AnalysisRunner.hpp"
#include "pipeline/FeatureCatalog.hpp"

using namespace glcm;

namespace {

const std::set<Type> ALL_FIRST_ORDER{Type::Minimum, Type::Maximum, Type::Range, Type::Median, Type::Percentile10, Type::Percentile90, Type::InterquartileRange, Type::MeanAbsoluteDeviation, Type::RobustMeanAbsoluteDeviation, Type::RootMeanSquared, Type::FirstOrderEnergy, Type::Variance, Type::Skewness, Type::Kurtosis, Type::FirstOrderEntropy, Type::Uniformity};

void ExpectClose(double actual, double expected) {
    EXPECT_NEAR(actual, expected, 1e-12 * std::max(1.0, std::abs(expected)));
}

// Ten pixels inside the mask, and a masked-out row that must not count
struct Region {
    cv::Mat gray = cv::Mat(3, 5, CV_16UC1, cv::Scalar(60000));
    cv::Mat mask = cv::Mat(3, 5, CV_8UC1, cv::Scalar(0));
    cv::Mat levels = cv::Mat(3, 5, CV_8UC1, cv::Scalar(0));

    Region(const std::vector<int>& values, const std::vector<int>& gray_levels) {
        for (int i = 0; i < 10; ++i) {
            gray.at<uint16_t>(i / 5, i % 5) = static_cast<uint16_t>(values[static_cast<size_t>(i)]);
            levels.at<uchar>(i / 5, i % 5) = static_cast<uchar>(gray_levels[static_cast<size_t>(i)]);
            mask.at<uchar>(i / 5, i % 5) = 255;
        }
    }
};

} // namespace

TEST(FirstOrderTest, MatchesNumPyOnASmallRegion) {
    // Expected values from NumPy: np.percentile (linear), population moments, entropy -sum(p * log(p + np.spacing(1)))
    const Region region({7, 1, 40, 2, 7, 12, 2, 20, 3, 7}, {2, 0, 3, 1, 2, 3, 1, 3, 1, 2});
    const auto values = ComputeFirstOrderStatistics(region.gray, region.mask, region.levels, 4, LogBase::Two, ALL_FIRST_ORDER);
    ASSERT_EQ(values.size(), ALL_FIRST_ORDER.size());
    ExpectClose(values.at(Type::Minimum), 1.0);
    ExpectClose(values.at(Type::Maximum), 40.0);
    ExpectClose(values.at(Type::Range), 39.0);
    ExpectClose(values.at(Type::Median), 7.0);
    ExpectClose(values.at(Type::Percentile10), 1.9);
    ExpectClose(values.at(Type::Percentile90), 21.999999999999993);
    ExpectClose(values.at(Type::InterquartileRange), 8.5);
    ExpectClose(values.at(Type::MeanAbsoluteDeviation), 8.34);
    ExpectClose(values.at(Type::RobustMeanAbsoluteDeviation), 4.25);
    ExpectClose(values.at(Type::RootMeanSquared), 15.195394038984313);
    ExpectClose(values.at(Type::FirstOrderEnergy), 2309.0);
    ExpectClose(values.at(Type::Variance), 128.89);
    ExpectClose(values.at(Type::Skewness), 1.738849574080599);
    ExpectClose(values.at(Type::Kurtosis), 4.979090200883941);
    ExpectClose(values.at(Type::FirstOrderEntropy), 1.8954618442383204);
    ExpectClose(values.at(Type::Uniformity), 0.28);

    const auto natural = ComputeFirstOrderStatistics(region.gray, region.mask, region.levels, 4, LogBase::Natural, {Type::FirstOrderEntropy});
    ExpectClose(natural.at(Type::FirstOrderEntropy), 1.313834033192746);
}

TEST(FirstOrderTest, HandlesConstantAndEmptyRegions) {
    const Region constant({5, 5, 5, 5, 5, 5, 5, 5, 5, 5}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
    const auto values = ComputeFirstOrderStatistics(constant.gray, constant.mask, constant.levels, 2, LogBase::Two, ALL_FIRST_ORDER);
    EXPECT_EQ(values.at(Type::Variance), 0.0);
    EXPECT_EQ(values.at(Type::Skewness), 0.0);
    EXPECT_EQ(values.at(Type::Kurtosis), 0.0);
    EXPECT_EQ(values.at(Type::RobustMeanAbsoluteDeviation), 0.0);
    EXPECT_EQ(values.at(Type::Uniformity), 1.0);
    EXPECT_NEAR(values.at(Type::FirstOrderEntropy), 0.0, 1e-15);

    const cv::Mat empty(3, 5, CV_8UC1, cv::Scalar(0));
    for (const auto& [type, value] : ComputeFirstOrderStatistics(constant.gray, empty, constant.levels, 2, LogBase::Two, ALL_FIRST_ORDER)) {
        EXPECT_TRUE(std::isnan(value)) << TextureAnalysis::TypeToString(type);
    }
}

TEST(FirstOrderTest, RejectsOtherFeaturesAndInvalidInput) {
    const Region region({7, 1, 40, 2, 7, 12, 2, 20, 3, 7}, {2, 0, 3, 1, 2, 3, 1, 3, 1, 2});
    EXPECT_TRUE(IsFirstOrderStatistic(Type::Skewness));
    EXPECT_FALSE(IsFirstOrderStatistic(Type::Mean));
    EXPECT_FALSE(IsFirstOrderStatistic(Type::Contrast));
    EXPECT_THROW(ComputeFirstOrderStatistics(region.gray, region.mask, region.levels, 4, LogBase::Two, {Type::Contrast}), std::invalid_argument);
    // A gray level of 3 with only 3 levels
    EXPECT_THROW(ComputeFirstOrderStatistics(region.gray, region.mask, region.levels, 3, LogBase::Two, {Type::Uniformity}), std::invalid_argument);
    EXPECT_THROW(ComputeFirstOrderStatistics(region.gray, region.mask(cv::Rect(0, 0, 4, 3)), region.levels, 4, LogBase::Two, {Type::Median}),
        std::invalid_argument);
}

TEST(FirstOrderTest, MatchesPyRadiomicsOnSampleImages) {
    // Written by scripts/radiomics-reference.py
    std::ifstream stream(GLCM_SOURCE_DIR "/core/tests/data/pyradiomics-firstorder.json");
    ASSERT_TRUE(stream) << "missing core/tests/data/pyradiomics-firstorder.json";
    const nlohmann::json document = nlohmann::json::parse(stream);
    ASSERT_GE(document.at("cases").size(), 3u);

    for (const nlohmann::json& reference : document.at("cases")) {
        const std::string image_path = reference.at("image");
        SCOPED_TRACE(image_path);
        const LoadedImage image = LoadImageFile(GLCM_SOURCE_DIR "/samples/" + image_path);
        const auto& rectangle = reference.at("rectangle");
        const cv::Rect box(rectangle.at(0), rectangle.at(1), rectangle.at(2), rectangle.at(3));
        const cv::Mat crop = image.gray(box);
        const cv::Mat mask(box.size(), CV_8UC1, cv::Scalar(255));
        // Quantization "none" with 256 levels: the gray levels of an 8-bit image are its intensities, as with a bin
        // width of 1 in PyRadiomics. 16-bit cases have no binned features.
        const cv::Mat levels = crop.depth() == CV_8U ? crop.clone() : cv::Mat(box.size(), CV_8UC1, cv::Scalar(0));

        std::set<Type> types;
        for (const auto& [id, value] : reference.at("features").items()) {
            if (id != "Mean") {
                types.insert(*FeatureTypeFromId(id));
            }
        }
        const auto values = ComputeFirstOrderStatistics(crop, mask, levels, 256, LogBase::Two, types);
        for (const auto& [id, value] : reference.at("features").items()) {
            const double expected = value.get<double>();
            // Sums in a different order (NumPy adds pairwise) change the last digits
            const double tolerance = 1e-9 * std::max(1.0, std::abs(expected));
            if (id == "Mean") {
                EXPECT_NEAR(ComputeRegionStatistics(crop, mask).mean, expected, tolerance);
                continue;
            }
            EXPECT_NEAR(values.at(*FeatureTypeFromId(id)), expected, tolerance) << id;
        }
    }
}
