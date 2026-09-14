#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <opencv2/opencv.hpp>
#include <set>
#include <stdexcept>

#include "analysis/Score.hpp"
#include "analysis/TextureAnalysis.hpp"

using glcm::Direction;
using glcm::Features;
using glcm::LogBase;
using glcm::TextureAnalysis;
using glcm::TextureOptions;
using glcm::Type;

namespace {

const Direction DIRECTIONS[] = {Direction::H, Direction::V, Direction::LD, Direction::RD};

// Deterministic image with gray levels in [0, Ng)
cv::Mat PatternImage(int rows, int cols, int Ng) {
    cv::Mat image(rows, cols, CV_8UC1);
    for (int m = 0; m < rows; ++m) {
        for (int n = 0; n < cols; ++n) {
            image.at<uchar>(m, n) = static_cast<uchar>((m * 7 + n * 3 + m * n) % Ng);
        }
    }
    return image;
}

std::set<Type> AllFeatureTypes() {
    std::set<Type> types;
    for (int t = static_cast<int>(Type::Mean); t < static_cast<int>(Type::Score); ++t) {
        types.insert(static_cast<Type>(t));
    }
    return types;
}

// Relative tolerance for values of very different magnitudes (cluster prominence is large, probabilities are small)
double Tolerance(double expected) {
    return 1e-12 * std::max(1.0, std::fabs(expected));
}

} // namespace

TEST(TextureOptionsTest, MaximalCorrelationCoefficientIsSelectableType) {
    TextureAnalysis analysis(8);
    analysis.ProcessRectImage(PatternImage(9, 9, 8), 1);
    Features direct;
    analysis.GetMaximalCorrelationCoefficient(direct);

    const Features calculated = analysis.Calculate({Type::MaximalCorrelationCoefficient}).at(Type::MaximalCorrelationCoefficient);
    for (Direction direction : DIRECTIONS) {
        EXPECT_EQ(calculated.Get(direction), direct.Get(direction));
    }
    EXPECT_EQ(TextureAnalysis::TypeToString(Type::MaximalCorrelationCoefficient), "Maximal Correlation Coefficient");
}

TEST(TextureOptionsTest, MaskedImageMatchesPolygonImage) {
    cv::Mat image = PatternImage(10, 10, 8);
    cv::Mat mask = cv::Mat::zeros(image.size(), CV_8UC1);
    mask(cv::Rect(2, 1, 6, 7)).setTo(cv::Scalar(255));

    TextureAnalysis masked(8);
    TextureAnalysis polygon(8);
    masked.ProcessMaskedImage(image, mask, 2);
    polygon.ProcessPolygonImage(image, mask, 2);

    const auto a = masked.Calculate(AllFeatureTypes());
    const auto b = polygon.Calculate(AllFeatureTypes());
    for (const auto& [type, values] : a) {
        for (Direction direction : DIRECTIONS) {
            EXPECT_EQ(values.Get(direction), b.at(type).Get(direction)) << TextureAnalysis::TypeToString(type);
        }
    }
}

TEST(TextureOptionsTest, InvalidInputsThrow) {
    TextureAnalysis analysis(8);
    cv::Mat image = PatternImage(5, 5, 8);

    EXPECT_THROW(analysis.ProcessMaskedImage(image, cv::Mat::zeros(4, 5, CV_8UC1), 1), std::invalid_argument);
    EXPECT_THROW(analysis.ProcessMaskedImage(image, cv::Mat::zeros(5, 5, CV_16UC1), 1), std::invalid_argument);
    EXPECT_THROW(analysis.ProcessRectImage(cv::Mat::zeros(5, 5, CV_16UC1), 1), std::invalid_argument);
    EXPECT_THROW(analysis.ProcessRectImage(image, 0), std::invalid_argument);
    EXPECT_THROW(analysis.PairCount(Direction::Avg), std::invalid_argument);

    TextureOptions no_directions;
    no_directions.directions = {};
    EXPECT_THROW(TextureAnalysis(8, no_directions), std::invalid_argument);

    TextureOptions average_direction;
    average_direction.directions = {Direction::Avg};
    EXPECT_THROW(TextureAnalysis(8, average_direction), std::invalid_argument);
}

TEST(TextureOptionsTest, DirectionSubsetLeavesOtherDirectionsNaN) {
    const cv::Mat image = PatternImage(9, 11, 8);
    TextureAnalysis all(8);
    TextureOptions options;
    options.directions = {Direction::H, Direction::RD};
    TextureAnalysis subset(8, options);
    all.ProcessRectImage(image, 1);
    subset.ProcessRectImage(image, 1);

    const auto full = all.Calculate(AllFeatureTypes());
    const auto part = subset.Calculate(AllFeatureTypes());
    for (const auto& [type, values] : full) {
        SCOPED_TRACE(TextureAnalysis::TypeToString(type));
        const Features& p = part.at(type);
        EXPECT_EQ(p.H, values.H);
        EXPECT_EQ(p.RD, values.RD);
        EXPECT_TRUE(std::isnan(p.V));
        EXPECT_TRUE(std::isnan(p.LD));

        const double expected_average = (values.H + values.RD) / 2.0;
        EXPECT_NEAR(p.Avg(), expected_average, Tolerance(expected_average));
        const double expected_range = std::fabs(values.H - values.RD);
        EXPECT_NEAR(p.Range(), expected_range, Tolerance(expected_range));
    }

    // 9 rows x 10 horizontal neighbors per row, each pair counted in both orders
    EXPECT_EQ(subset.PairCount(Direction::H), 9 * 10 * 2);
    EXPECT_EQ(subset.PairCount(Direction::V), 0);
    EXPECT_EQ(all.PairCount(Direction::V), 8 * 11 * 2);
}

TEST(TextureOptionsTest, AverageAndRangeIgnoreMissingDirections) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Features features{1.0, nan, 4.0, nan};
    EXPECT_DOUBLE_EQ(features.Avg(), 2.5);
    EXPECT_DOUBLE_EQ(features.Range(), 3.0);
    EXPECT_DOUBLE_EQ(features.Get(Direction::Avg), 2.5);

    Features empty{nan, nan, nan, nan};
    EXPECT_TRUE(std::isnan(empty.Avg()));
    EXPECT_TRUE(std::isnan(empty.Range()));
}

TEST(TextureOptionsTest, LogBaseTwoScalesEntropies) {
    const cv::Mat image = PatternImage(9, 9, 8);
    TextureAnalysis natural(8);
    TextureOptions options;
    options.log_base = LogBase::Two;
    TextureAnalysis base_two(8, options);
    natural.ProcessRectImage(image, 1);
    base_two.ProcessRectImage(image, 1);

    const std::set<Type> types{Type::Entropy, Type::SumEntropy, Type::DifferenceEntropy, Type::InformationMeasuresOfCorrelationI,
        Type::InformationMeasuresOfCorrelationII, Type::Contrast};
    const auto n = natural.Calculate(types);
    const auto t = base_two.Calculate(types);
    const double ln2 = std::log(2.0);

    for (Direction direction : DIRECTIONS) {
        for (Type type : {Type::Entropy, Type::SumEntropy, Type::DifferenceEntropy}) {
            EXPECT_NEAR(t.at(type).Get(direction), n.at(type).Get(direction) / ln2, 1e-12);
        }
        // IMC1 is a ratio of entropies, so the log base cancels
        EXPECT_NEAR(t.at(Type::InformationMeasuresOfCorrelationI).Get(direction),
            n.at(Type::InformationMeasuresOfCorrelationI).Get(direction), 1e-12);
        EXPECT_EQ(t.at(Type::Contrast).Get(direction), n.at(Type::Contrast).Get(direction));

        // IMC2 = sqrt(1 - exp(-2 (HXY2 - HXY))): recover the natural-log difference, convert it to bits, re-apply
        const double imc2_natural = n.at(Type::InformationMeasuresOfCorrelationII).Get(direction);
        const double difference_natural = -std::log(1.0 - imc2_natural * imc2_natural) / 2.0;
        const double expected = std::sqrt(1.0 - std::exp(-2.0 * difference_natural / ln2));
        EXPECT_NEAR(t.at(Type::InformationMeasuresOfCorrelationII).Get(direction), expected, 1e-9);
    }
}

TEST(ScoreTest, ComputeScoreUsesCoefficients) {
    const Features mean{10.0, 20.0, 30.0, 40.0};
    const Features entropy{1.0, 2.0, 3.0, 4.0};
    const Features contrast{0.5, 0.25, 0.125, 0.0625};
    const glcm::ScoreCoefficients coefficients{2.0, -1.0, 3.0, 4.0};

    const Features score = glcm::ComputeScore(50.0, mean, entropy, contrast, coefficients);
    EXPECT_DOUBLE_EQ(score.H, 2.0 * 50.0 - 10.0 + 3.0 * 1.0 + 4.0 * 0.5);
    EXPECT_DOUBLE_EQ(score.V, 2.0 * 50.0 - 20.0 + 3.0 * 2.0 + 4.0 * 0.25);
    EXPECT_DOUBLE_EQ(score.LD, 2.0 * 50.0 - 30.0 + 3.0 * 3.0 + 4.0 * 0.125);
    EXPECT_DOUBLE_EQ(score.RD, 2.0 * 50.0 - 40.0 + 3.0 * 4.0 + 4.0 * 0.0625);
}

TEST(ScoreTest, LegacyCalculateScoreUsesDefaultCoefficients) {
    const Features mean{82.7, 82.7, 82.7, 82.7};
    const Features entropy{7.1, 7.3, 7.2, 7.4};
    const Features contrast{33.3, 23.1, 48.4, 41.4};
    std::map<Type, Features> features{{Type::Mean, mean}, {Type::Entropy, entropy}, {Type::Contrast, contrast}};

    TextureAnalysis analysis(8);
    analysis.CalculateScore(40.0, features);

    const glcm::ScoreCoefficients defaults;
    EXPECT_DOUBLE_EQ(defaults.age, 1.138);
    EXPECT_DOUBLE_EQ(defaults.mean, -1.814);
    EXPECT_DOUBLE_EQ(defaults.entropy, 1.416);
    EXPECT_DOUBLE_EQ(defaults.contrast, 1.714);

    const Features expected = glcm::ComputeScore(40.0, mean, entropy, contrast);
    for (Direction direction : DIRECTIONS) {
        EXPECT_DOUBLE_EQ(features.at(Type::Score).Get(direction), expected.Get(direction));
        EXPECT_DOUBLE_EQ(features.at(Type::Age).Get(direction), 40.0);
    }
    EXPECT_DOUBLE_EQ(features.at(Type::Score).H, 1.138 * 40.0 - 1.814 * 82.7 + 1.416 * 7.1 + 1.714 * 33.3);
}
