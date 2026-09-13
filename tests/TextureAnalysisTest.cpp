#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <opencv2/opencv.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "analysis/TextureAnalysis.hpp"

using glcm::Features;
using glcm::TextureAnalysis;
using glcm::Type;

namespace {

using Matrix = std::vector<std::vector<double>>;

const int WHITE = 255;
const double TOLERANCE = 1e-9;

const std::set<Type> REFERENCE_TYPES{Type::Mean, Type::Std, Type::Contrast, Type::Energy, Type::Entropy, Type::InverseDifferenceNormalized,
    Type::InverseDifferenceMomentNormalized};

// Neighbor offsets (row, col) of each direction, indexed H = 0, V = 1, LD = 2, RD = 3
std::vector<std::pair<int, int>> Offsets(int direction, int d) {
    switch (direction) {
        case 0:
            return {{0, d}, {0, -d}};
        case 1:
            return {{d, 0}, {-d, 0}};
        case 2:
            return {{d, d}, {-d, -d}};
        default:
            return {{d, -d}, {-d, d}};
    }
}

double Component(const Features& f, int direction) {
    const double values[] = {f.H, f.V, f.LD, f.RD};
    return values[direction];
}

// Straightforward GLCM: count every pair whose two pixels are both inside the mask, then normalize
Matrix ReferenceGlcm(const cv::Mat& image, const cv::Mat& mask, int Ng, int direction, int d) {
    Matrix p(Ng, std::vector<double>(Ng, 0.0));
    double total = 0.0;
    for (int m = 0; m < image.rows; ++m) {
        for (int n = 0; n < image.cols; ++n) {
            if (mask.at<uchar>(m, n) != WHITE) {
                continue;
            }
            for (auto [dr, dc] : Offsets(direction, d)) {
                int k = m + dr;
                int l = n + dc;
                if (k < 0 || l < 0 || k >= image.rows || l >= image.cols || mask.at<uchar>(k, l) != WHITE) {
                    continue;
                }
                p[image.at<uchar>(k, l)][image.at<uchar>(m, n)] += 1.0;
                total += 1.0;
            }
        }
    }
    for (auto& row : p) {
        for (auto& value : row) {
            value = (total > 0) ? value / total : 0.0;
        }
    }
    return p;
}

void ExpectMatchesReference(const std::map<Type, Features>& results, const cv::Mat& image, const cv::Mat& mask, int Ng, int d) {
    // Mean and sample STD of the pixels inside the mask
    std::vector<double> pixels;
    for (int m = 0; m < image.rows; ++m) {
        for (int n = 0; n < image.cols; ++n) {
            if (mask.at<uchar>(m, n) == WHITE) {
                pixels.push_back(image.at<uchar>(m, n));
            }
        }
    }
    double mean = 0.0;
    for (double v : pixels) {
        mean += v / pixels.size();
    }
    double variance = 0.0;
    for (double v : pixels) {
        variance += (v - mean) * (v - mean) / (pixels.size() - 1.0);
    }

    for (int direction = 0; direction < 4; ++direction) {
        SCOPED_TRACE("direction " + std::to_string(direction));
        Matrix p = ReferenceGlcm(image, mask, Ng, direction, d);

        double contrast = 0.0;
        double energy = 0.0;
        double entropy = 0.0;
        double idn = 0.0;
        double idmn = 0.0;
        for (int i = 0; i < Ng; ++i) {
            for (int j = 0; j < Ng; ++j) {
                contrast += (i - j) * (i - j) * p[i][j];
                energy += p[i][j] * p[i][j];
                entropy -= (p[i][j] > 0) ? p[i][j] * std::log(p[i][j]) : 0.0;
                idn += p[i][j] / (1.0 + std::abs(i - j) / static_cast<double>(Ng));
                idmn += p[i][j] / (1.0 + (i - j) * (i - j) / static_cast<double>(Ng * Ng));
            }
        }

        EXPECT_NEAR(Component(results.at(Type::Mean), direction), mean, TOLERANCE);
        EXPECT_NEAR(Component(results.at(Type::Std), direction), std::sqrt(variance), TOLERANCE);
        EXPECT_NEAR(Component(results.at(Type::Contrast), direction), contrast, TOLERANCE);
        EXPECT_NEAR(Component(results.at(Type::Energy), direction), energy, TOLERANCE);
        EXPECT_NEAR(Component(results.at(Type::Entropy), direction), entropy, TOLERANCE);
        EXPECT_NEAR(Component(results.at(Type::InverseDifferenceNormalized), direction), idn, TOLERANCE);
        EXPECT_NEAR(Component(results.at(Type::InverseDifferenceMomentNormalized), direction), idmn, TOLERANCE);
    }
}

void ExpectSameFeatures(const std::map<Type, Features>& a, const std::map<Type, Features>& b) {
    ASSERT_EQ(a.size(), b.size());
    for (const auto& [type, features] : a) {
        for (int direction = 0; direction < 4; ++direction) {
            EXPECT_NEAR(Component(features, direction), Component(b.at(type), direction), TOLERANCE)
                << "type " << static_cast<int>(type) << ", direction " << direction;
        }
    }
}

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

cv::Mat FullMask(const cv::Mat& image) {
    return cv::Mat(image.size(), CV_8UC1, cv::Scalar(WHITE));
}

bool AllFinite(const std::map<Type, Features>& results) {
    for (const auto& [type, features] : results) {
        for (int direction = 0; direction < 4; ++direction) {
            if (!std::isfinite(Component(features, direction))) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

// Worked example from Haralick et al. (1973): 0 degree GLCM = [[4,2,1,0],[2,4,0,0],[1,0,6,1],[0,0,1,2]] / 24
TEST(TextureAnalysisTest, HaralickExampleHorizontal) {
    uchar pixels[] = {0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 3, 3};
    cv::Mat image = cv::Mat(4, 4, CV_8UC1, pixels).clone();
    TextureAnalysis texture_analysis(4);
    texture_analysis.ProcessRectImage(image, 1);
    auto results = texture_analysis.Calculate({Type::Mean, Type::Contrast, Type::Energy});

    EXPECT_NEAR(results.at(Type::Contrast).H, 14.0 / 24.0, TOLERANCE);
    EXPECT_NEAR(results.at(Type::Energy).H, 84.0 / 576.0, TOLERANCE);
    EXPECT_NEAR(results.at(Type::Mean).H, 20.0 / 16.0, TOLERANCE);
}

TEST(TextureAnalysisTest, RectMatchesReferenceForDistances) {
    const int Ng = 8;
    cv::Mat image = PatternImage(7, 9, Ng);
    TextureAnalysis texture_analysis(Ng);
    for (int d : {1, 2, 3}) {
        SCOPED_TRACE("distance " + std::to_string(d));
        texture_analysis.ProcessRectImage(image, d);
        ExpectMatchesReference(texture_analysis.Calculate(REFERENCE_TYPES), image, FullMask(image), Ng, d);
    }
}

TEST(TextureAnalysisTest, ConstantImage) {
    cv::Mat image(5, 5, CV_8UC1, cv::Scalar(5));
    TextureAnalysis texture_analysis(8);
    texture_analysis.ProcessRectImage(image, 1);
    auto results = texture_analysis.Calculate(REFERENCE_TYPES);

    EXPECT_NEAR(results.at(Type::Mean).Avg(), 5.0, TOLERANCE);
    EXPECT_NEAR(results.at(Type::Std).Avg(), 0.0, TOLERANCE);
    EXPECT_NEAR(results.at(Type::Contrast).Avg(), 0.0, TOLERANCE);
    EXPECT_NEAR(results.at(Type::Energy).Avg(), 1.0, TOLERANCE);
    EXPECT_NEAR(results.at(Type::Entropy).Avg(), 0.0, TOLERANCE);
}

TEST(TextureAnalysisTest, PolygonWithFullMaskEqualsRect) {
    const int Ng = 8;
    cv::Mat image = PatternImage(6, 8, Ng);
    TextureAnalysis rect_analysis(Ng);
    TextureAnalysis polygon_analysis(Ng);

    rect_analysis.ProcessRectImage(image, 1);
    polygon_analysis.ProcessPolygonImage(image, FullMask(image), 1);

    ExpectSameFeatures(rect_analysis.Calculate(REFERENCE_TYPES), polygon_analysis.Calculate(REFERENCE_TYPES));
}

// Regression: pixels outside the mask used to be counted as the central pixel of a pair
TEST(TextureAnalysisTest, PolygonIgnoresPixelsOutsideMask) {
    const int Ng = 8;
    cv::Mat image = PatternImage(6, 6, Ng);
    image(cv::Rect(3, 0, 3, 6)).setTo(cv::Scalar(Ng - 1)); // right half differs from the left half
    cv::Mat mask = cv::Mat::zeros(image.size(), CV_8UC1);
    mask(cv::Rect(0, 0, 3, 6)).setTo(cv::Scalar(WHITE)); // keep only the left half

    TextureAnalysis rect_analysis(Ng);
    TextureAnalysis polygon_analysis(Ng);
    rect_analysis.ProcessRectImage(image(cv::Rect(0, 0, 3, 6)).clone(), 1);
    polygon_analysis.ProcessPolygonImage(image, mask, 1);

    ExpectSameFeatures(rect_analysis.Calculate(REFERENCE_TYPES), polygon_analysis.Calculate(REFERENCE_TYPES));
}

TEST(TextureAnalysisTest, PolygonMatchesReferenceForTriangle) {
    const int Ng = 8;
    cv::Mat image = PatternImage(10, 10, Ng);
    cv::Mat mask = cv::Mat::zeros(image.size(), CV_8UC1);
    std::vector<std::vector<cv::Point>> triangle{{{1, 1}, {8, 2}, {3, 9}}};
    cv::fillPoly(mask, triangle, cv::Scalar(WHITE));

    TextureAnalysis texture_analysis(Ng);
    for (int d : {1, 2}) {
        SCOPED_TRACE("distance " + std::to_string(d));
        texture_analysis.ProcessPolygonImage(image, mask, d);
        ExpectMatchesReference(texture_analysis.Calculate(REFERENCE_TYPES), image, mask, Ng, d);
    }
}

// Regression: directions without any pixel pair used to divide by zero
TEST(TextureAnalysisTest, SingleRowRegionHasNoNaN) {
    uchar pixels[] = {1, 2, 3, 4, 5};
    cv::Mat image = cv::Mat(1, 5, CV_8UC1, pixels).clone();
    TextureAnalysis texture_analysis(8);
    texture_analysis.ProcessRectImage(image, 1);
    auto results = texture_analysis.Calculate(REFERENCE_TYPES);

    EXPECT_TRUE(AllFinite(results));
    EXPECT_NEAR(results.at(Type::Contrast).H, 1.0, TOLERANCE);
    EXPECT_NEAR(results.at(Type::Contrast).V, 0.0, TOLERANCE);
    EXPECT_NEAR(results.at(Type::Std).H, std::sqrt(2.5), TOLERANCE);
}

TEST(TextureAnalysisTest, SinglePixelRegionHasNoNaN) {
    cv::Mat image(1, 1, CV_8UC1, cv::Scalar(3));
    TextureAnalysis texture_analysis(8);
    texture_analysis.ProcessRectImage(image, 1);
    auto results = texture_analysis.Calculate(REFERENCE_TYPES);

    EXPECT_TRUE(AllFinite(results));
    EXPECT_NEAR(results.at(Type::Mean).H, 3.0, TOLERANCE);
    EXPECT_NEAR(results.at(Type::Std).H, 0.0, TOLERANCE);
}

// Regression: the entropy factors used to accumulate across Calculate calls
TEST(TextureAnalysisTest, InformationMeasuresAreStableAcrossCalls) {
    const int Ng = 8;
    cv::Mat image = PatternImage(8, 8, Ng);
    TextureAnalysis texture_analysis(Ng);
    texture_analysis.ProcessRectImage(image, 1);

    std::set<Type> types{Type::InformationMeasuresOfCorrelationI, Type::InformationMeasuresOfCorrelationII};
    auto first = texture_analysis.Calculate(types);
    auto second = texture_analysis.Calculate(types);

    ExpectSameFeatures(first, second);
}

TEST(TextureAnalysisTest, SumVarianceIsCenteredOnSumAverage) {
    const int Ng = 8;
    cv::Mat image = PatternImage(7, 9, Ng);
    TextureAnalysis texture_analysis(Ng);
    texture_analysis.ProcessRectImage(image, 1);
    auto results = texture_analysis.Calculate({Type::SumAverage, Type::SumVariance});

    for (int direction = 0; direction < 4; ++direction) {
        SCOPED_TRACE("direction " + std::to_string(direction));
        Matrix p = ReferenceGlcm(image, FullMask(image), Ng, direction, 1);

        std::vector<double> p_xpy(2 * Ng - 1, 0.0);
        for (int i = 0; i < Ng; ++i) {
            for (int j = 0; j < Ng; ++j) {
                p_xpy[i + j] += p[i][j];
            }
        }
        double average = 0.0;
        for (int k = 0; k < (int)p_xpy.size(); ++k) {
            average += k * p_xpy[k];
        }
        double variance = 0.0;
        for (int k = 0; k < (int)p_xpy.size(); ++k) {
            variance += (k - average) * (k - average) * p_xpy[k];
        }

        EXPECT_NEAR(Component(results.at(Type::SumAverage), direction), average, TOLERANCE);
        EXPECT_NEAR(Component(results.at(Type::SumVariance), direction), variance, TOLERANCE);
    }
}
