#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <vector>

#include "analysis/TextureAnalysis.hpp"
#include "imaging/Quantizer.hpp"
#include "io/Json.hpp"
#include "pipeline/FeatureMap.hpp"

using namespace glcm;

namespace {

cv::Mat Pattern16(int rows, int cols) {
    cv::Mat image(rows, cols, CV_16UC1);
    for (int m = 0; m < rows; ++m) {
        for (int n = 0; n < cols; ++n) {
            image.at<uint16_t>(m, n) = static_cast<uint16_t>(1000 + (m * 3671 + n * 977 + m * n * 131) % 40000);
        }
    }
    return image;
}

cv::Mat Pattern8(int rows, int cols) {
    cv::Mat image(rows, cols, CV_8UC1);
    for (int m = 0; m < rows; ++m) {
        for (int n = 0; n < cols; ++n) {
            image.at<uchar>(m, n) = static_cast<uchar>((m * 37 + n * 11 + m * n * 3) % 256);
        }
    }
    return image;
}

// The map value at a grid point, computed directly from the whole quantized image
float DirectValue(const cv::Mat& gray, const FeatureMapSettings& settings, int step, int column, int row) {
    const cv::Mat mask(gray.size(), CV_8UC1, cv::Scalar(255));
    const cv::Mat levels = Quantize(gray, mask, settings.gray_levels, settings.quantization).image;
    const auto centre = [step](int index, int size) {
        const int first = index * step;
        const int last = std::min(first + step, size) - 1;
        return (first + last) / 2;
    };
    const int half = settings.window / 2;
    const int x = centre(column, gray.cols);
    const int y = centre(row, gray.rows);
    const cv::Rect window = cv::Rect(x - half, y - half, settings.window, settings.window) & cv::Rect(0, 0, gray.cols, gray.rows);

    TextureOptions options;
    options.directions = settings.directions;
    options.log_base = settings.log_base;
    TextureAnalysis analysis(settings.gray_levels, options);
    analysis.ProcessRectImage(levels(window).clone(), settings.distance);
    for (Direction direction : settings.directions) {
        if (analysis.PairCount(direction) == 0) {
            return std::nanf("");
        }
    }
    return static_cast<float>(analysis.Calculate({settings.feature}).at(settings.feature).Avg());
}

void ExpectSameValues(const std::vector<float>& actual, const std::vector<float>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < actual.size(); ++i) {
        if (std::isnan(expected[i])) {
            EXPECT_TRUE(std::isnan(actual[i])) << "value " << i;
        } else {
            EXPECT_EQ(actual[i], expected[i]) << "value " << i;
        }
    }
}

} // namespace

TEST(FeatureMapTest, EachPointIsTheFeatureOfItsWindowInTheWholeQuantizedImage) {
    const cv::Mat gray = Pattern16(23, 31);
    for (QuantizationMethod method : {QuantizationMethod::FixedRange, QuantizationMethod::RoiMinMax, QuantizationMethod::FixedBinWidth}) {
        FeatureMapSettings settings;
        settings.feature = Type::Entropy;
        settings.window = 7;
        settings.step = 3;
        settings.gray_levels = 16;
        settings.quantization.method = method;
        settings.quantization.range_min = 5000;
        settings.quantization.range_max = 30000;
        settings.quantization.bin_width = 2600.0;
        settings.distance = 2;
        settings.directions = {Direction::H, Direction::RD};
        settings.log_base = LogBase::Two;

        const FeatureMapGrid grid = ResolveFeatureMapGrid(gray.cols, gray.rows, settings.step);
        ASSERT_EQ(grid.columns, 11);
        ASSERT_EQ(grid.rows, 8);
        std::vector<float> expected;
        for (int row = 0; row < grid.rows; ++row) {
            for (int column = 0; column < grid.columns; ++column) {
                expected.push_back(DirectValue(gray, settings, grid.step, column, row));
            }
        }
        ExpectSameValues(ComputeFeatureMapRows(gray, settings, 0, grid.rows), expected);
    }
}

TEST(FeatureMapTest, RowsComputedInBandsEqualTheWholeMap) {
    const cv::Mat gray = Pattern16(40, 17);
    FeatureMapSettings settings;
    settings.window = 5;
    settings.step = 2;
    settings.quantization.method = QuantizationMethod::FixedBinWidth;
    settings.quantization.bin_width = 1300.0;
    const std::vector<float> whole = ComputeFeatureMapRows(gray, settings, 0, 20);
    for (int band : {1, 3, 7}) {
        std::vector<float> joined;
        for (int first = 0; first < 20; first += band) {
            const std::vector<float> part = ComputeFeatureMapRows(gray, settings, first, std::min(band, 20 - first));
            joined.insert(joined.end(), part.begin(), part.end());
        }
        ExpectSameValues(joined, whole);
    }
}

TEST(FeatureMapTest, ContrastOfAnEightBitImageAtStepOne) {
    const cv::Mat gray = Pattern8(9, 12);
    FeatureMapSettings settings;
    settings.window = 3;
    settings.step = 1;
    settings.gray_levels = 8;
    const std::vector<float> values = ComputeFeatureMapRows(gray, settings, 0, 9);
    ASSERT_EQ(values.size(), 108u);
    for (int row = 0; row < 9; row += 4) {
        for (int column = 0; column < 12; column += 5) {
            const float expected = DirectValue(gray, settings, 1, column, row);
            EXPECT_EQ(values[row * 12 + column], expected);
            EXPECT_TRUE(std::isfinite(expected));
        }
    }
}

TEST(FeatureMapTest, WindowsWithoutPairsAreNan) {
    const cv::Mat gray = Pattern8(10, 10);
    FeatureMapSettings settings;
    settings.window = 3;
    settings.step = 1;
    settings.distance = 2;
    settings.directions = {Direction::H};
    const std::vector<float> values = ComputeFeatureMapRows(gray, settings, 0, 10);
    // The corner window is 2 pixels wide: no horizontal pairs at distance 2; an inner window is 3 wide
    EXPECT_TRUE(std::isnan(values[0]));
    EXPECT_TRUE(std::isfinite(values[5 * 10 + 5]));
}

TEST(FeatureMapTest, AutomaticStepKeepsAtMost512PointsPerSide) {
    EXPECT_EQ(AutomaticFeatureMapStep(512, 100), 1);
    EXPECT_EQ(AutomaticFeatureMapStep(513, 10), 2);
    EXPECT_EQ(AutomaticFeatureMapStep(10, 4096), 8);
    const FeatureMapGrid automatic = ResolveFeatureMapGrid(1025, 300, 0);
    EXPECT_EQ(automatic.step, 3);
    EXPECT_EQ(automatic.columns, 342);
    EXPECT_EQ(automatic.rows, 100);
    const FeatureMapGrid manual = ResolveFeatureMapGrid(10, 7, 4);
    EXPECT_EQ(manual.columns, 3);
    EXPECT_EQ(manual.rows, 2);
    EXPECT_EQ(ResolveFeatureMapGrid(5, 5, 100).columns, 1);
    EXPECT_THROW(ResolveFeatureMapGrid(5000, 10, 2), std::invalid_argument);
    EXPECT_NO_THROW(ResolveFeatureMapGrid(5000, 10, 3));
    EXPECT_THROW(ResolveFeatureMapGrid(10, 10, -1), std::invalid_argument);
}

TEST(FeatureMapTest, RejectsInvalidSettings) {
    const auto invalid = [](void (*change)(FeatureMapSettings&)) {
        FeatureMapSettings settings;
        change(settings);
        EXPECT_THROW(ValidateFeatureMapSettings(settings), std::invalid_argument);
    };
    EXPECT_NO_THROW(ValidateFeatureMapSettings(FeatureMapSettings{}));
    EXPECT_TRUE(IsFeatureMapFeature(Type::ClusterShade));
    EXPECT_FALSE(IsFeatureMapFeature(Type::MaximalCorrelationCoefficient));
    EXPECT_FALSE(IsFeatureMapFeature(Type::Mean));
    EXPECT_FALSE(IsFeatureMapFeature(Type::GlrlmRunEntropy));
    invalid([](FeatureMapSettings& s) { s.feature = Type::MaximalCorrelationCoefficient; });
    invalid([](FeatureMapSettings& s) { s.feature = Type::FirstOrderEntropy; });
    invalid([](FeatureMapSettings& s) { s.feature = Type::Score; });
    invalid([](FeatureMapSettings& s) { s.window = 8; });
    invalid([](FeatureMapSettings& s) { s.window = 1; });
    invalid([](FeatureMapSettings& s) { s.window = MAX_FEATURE_MAP_WINDOW + 2; });
    invalid([](FeatureMapSettings& s) { s.distance = 15; });
    invalid([](FeatureMapSettings& s) { s.gray_levels = 300; });
    invalid([](FeatureMapSettings& s) { s.directions.clear(); });
    invalid([](FeatureMapSettings& s) { s.step = -2; });

    const cv::Mat gray = Pattern8(6, 6);
    FeatureMapSettings none;
    none.quantization.method = QuantizationMethod::None;
    EXPECT_THROW(ComputeFeatureMapRows(gray, none, 0, 1), std::invalid_argument);
    EXPECT_THROW(ComputeFeatureMapRows(gray, FeatureMapSettings{}, 5, 2), std::invalid_argument);
}

TEST(FeatureMapTest, ReadsSettingsFromJson) {
    const FeatureMapSettings settings = FeatureMapSettingsFromJson(
        R"({"feature": "Entropy", "window": 9, "step": null, "grayLevels": 64, "distance": 2, "directions": [0, 90],
            "logBase": "log2", "quantization": {"method": "fixedBinWidth", "min": 0, "max": 255, "binWidth": 4}})");
    EXPECT_EQ(settings.feature, Type::Entropy);
    EXPECT_EQ(settings.window, 9);
    EXPECT_EQ(settings.step, 0);
    EXPECT_EQ(settings.gray_levels, 64);
    EXPECT_EQ(settings.distance, 2);
    EXPECT_EQ(settings.directions, (std::set<Direction>{Direction::H, Direction::V}));
    EXPECT_EQ(settings.log_base, LogBase::Two);
    EXPECT_EQ(settings.quantization.method, QuantizationMethod::FixedBinWidth);
    EXPECT_EQ(settings.quantization.bin_width, 4.0);
    EXPECT_EQ(FeatureMapSettingsFromJson(R"({"feature": "Contrast", "step": 5})").step, 5);

    const auto message = [](const char* text) {
        try {
            FeatureMapSettingsFromJson(text);
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }
        return std::string();
    };
    EXPECT_EQ(message(R"({"feature": "Nope"})"), "Invalid feature map settings: settings.feature \"Nope\" is not a known feature");
    EXPECT_EQ(message(R"({"window": 3})"), "Invalid feature map settings: settings.feature is missing");
    EXPECT_EQ(message(R"({"feature": "Contrast", "directions": [10]})"),
        "Invalid feature map settings: settings.directions[0] must be 0, 45, 90 or 135");
    EXPECT_EQ(message(R"({"feature": "Contrast", "window": 2.5})"), "Invalid feature map settings: settings.window must be an integer");
}

TEST(FeatureMapTest, StopsWhenCancelled) {
    const cv::Mat gray = Pattern8(20, 20);
    FeatureMapSettings settings;
    settings.step = 1;
    std::atomic<bool> cancel{true};
    EXPECT_THROW(ComputeFeatureMapRows(gray, settings, 0, 20, &cancel), FeatureMapCancelled);
    cancel = false;
    EXPECT_EQ(ComputeFeatureMapRows(gray, settings, 0, 20, &cancel).size(), 400u);
}

TEST(FeatureMapTest, EstimatesRowWorkFromPointsWindowGrayLevelsAndDirections) {
    FeatureMapSettings settings;
    settings.window = 15;
    settings.step = 4;
    settings.gray_levels = 32;
    const double work = FeatureMapRowWork(settings, 400, 100);
    // 100 points, each 225 pixels × 9 pair visits and 4 directions × 1024 cells × 4 passes
    EXPECT_DOUBLE_EQ(work, 100.0 * (225.0 * 9.0 + 4.0 * 1024.0 * 4.0));

    FeatureMapSettings denser = settings;
    denser.step = 2;
    EXPECT_DOUBLE_EQ(FeatureMapRowWork(denser, 400, 100), 2.0 * work);
    FeatureMapSettings finer = settings;
    finer.gray_levels = 256;
    EXPECT_GT(FeatureMapRowWork(finer, 400, 100), 10.0 * work);
    FeatureMapSettings one_direction = settings;
    one_direction.directions = {Direction::V};
    EXPECT_LT(FeatureMapRowWork(one_direction, 400, 100), work / 3.0);

    // Windows larger than the image are clipped to it
    FeatureMapSettings large = settings;
    large.window = 127;
    FeatureMapSettings small = settings;
    small.window = 11;
    EXPECT_DOUBLE_EQ(FeatureMapRowWork(large, 10, 10), FeatureMapRowWork(small, 10, 10));
    EXPECT_THROW(FeatureMapRowWork(settings, 0, 10), std::invalid_argument);
}
