#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <vector>

#include "imaging/EdgeDetection.hpp"
#include "roi/Livewire.hpp"

using namespace glcm;

namespace {

// Dark background with a bright square [10, 30) x [10, 30)
cv::Mat Square(int depth = CV_8U) {
    cv::Mat image(40, 40, depth, cv::Scalar(20));
    image(cv::Rect(10, 10, 20, 20)).setTo(cv::Scalar(depth == CV_16U ? 40000 : 220));
    return image;
}

} // namespace

TEST(EdgeDetectionTest, GradientMagnitudeIsInIntensityUnitsPerPixel) {
    cv::Mat ramp(20, 30, CV_16UC1);
    for (int y = 0; y < ramp.rows; ++y) {
        for (int x = 0; x < ramp.cols; ++x) {
            ramp.at<uint16_t>(y, x) = static_cast<uint16_t>(1000 + 300 * x);
        }
    }
    const cv::Mat magnitude = GradientMagnitude(ramp, 0.0);
    ASSERT_EQ(magnitude.type(), CV_32FC1);
    EXPECT_FLOAT_EQ(magnitude.at<float>(10, 15), 300.0F);
    // Smoothing keeps a linear ramp, up to single-precision rounding
    EXPECT_NEAR(GradientMagnitude(ramp, 2.0).at<float>(10, 15), 300.0F, 1e-3);

    const cv::Mat flat(10, 10, CV_8UC1, cv::Scalar(77));
    EXPECT_EQ(cv::countNonZero(GradientMagnitude(flat, 1.0)), 0);
    const GradientStatistics statistics = ComputeGradientStatistics(ramp, 0.0);
    EXPECT_DOUBLE_EQ(statistics.p50, 300.0);
    EXPECT_DOUBLE_EQ(statistics.max, 300.0);
}

TEST(EdgeDetectionTest, StatisticsDescribeTheGradientDistribution) {
    const GradientStatistics statistics = ComputeGradientStatistics(Square(), 0.0);
    // Most pixels are flat; the edges around the square are strong
    EXPECT_DOUBLE_EQ(statistics.p50, 0.0);
    EXPECT_GT(statistics.p99, 50.0);
    EXPECT_LE(statistics.p90, statistics.p95);
    EXPECT_LE(statistics.p95, statistics.p99);
    EXPECT_LE(statistics.p99, statistics.max);
}

TEST(EdgeDetectionTest, SobelMapsTheMagnitudeWindowToEightBits) {
    const cv::Mat gray = Square();
    const cv::Mat magnitude = GradientMagnitude(gray, 0.0);
    const cv::Mat map = RenderEdgeMap(gray, EdgeMethod::Sobel, 0.0, 10.0, 60.0);
    ASSERT_EQ(map.type(), CV_8UC1);
    for (const cv::Point& point : {cv::Point(0, 0), cv::Point(10, 20), cv::Point(9, 20), cv::Point(20, 20), cv::Point(10, 10)}) {
        const double expected = std::clamp(std::round((magnitude.at<float>(point) - 10.0) * 255.0 / 50.0), 0.0, 255.0);
        EXPECT_EQ(map.at<uchar>(point), expected) << point;
    }
    EXPECT_EQ(RenderEdgeMap(gray, EdgeMethod::Sobel, 0.0, 0.0, 100.0, 20).size(), cv::Size(20, 20));
}

TEST(EdgeDetectionTest, CannyFindsThinEdgesAroundTheSquare) {
    for (int depth : {CV_8U, CV_16U}) {
        const cv::Mat gray = Square(depth);
        const double step = depth == CV_16U ? 40000.0 - 20.0 : 200.0;
        const cv::Mat edges = RenderEdgeMap(gray, EdgeMethod::Canny, 1.0, 0.1 * step, 0.3 * step);
        ASSERT_EQ(edges.type(), CV_8UC1);
        // Every row through the middle of the square has one edge pixel at each vertical side, and none far from it
        for (int y = 14; y < 26; ++y) {
            int left = 0;
            int right = 0;
            for (int x = 0; x < gray.cols; ++x) {
                if (edges.at<uchar>(y, x) == 255) {
                    EXPECT_TRUE(std::abs(x - 10) <= 1 || std::abs(x - 29) <= 1) << "row " << y << " column " << x;
                    (x < 20 ? left : right) += 1;
                }
            }
            EXPECT_EQ(left, 1) << "row " << y;
            EXPECT_EQ(right, 1) << "row " << y;
        }
        EXPECT_EQ(cv::countNonZero(edges(cv::Rect(14, 14, 12, 12))), 0);
        // Thresholds above every gradient find nothing
        EXPECT_EQ(cv::countNonZero(RenderEdgeMap(gray, EdgeMethod::Canny, 1.0, 2.0 * step, 3.0 * step)), 0);
        // Reduced maps keep the edges
        const cv::Mat reduced = RenderEdgeMap(gray, EdgeMethod::Canny, 1.0, 0.1 * step, 0.3 * step, 10);
        EXPECT_EQ(reduced.size(), cv::Size(10, 10));
        EXPECT_GT(cv::countNonZero(reduced), 0);
    }
}

TEST(EdgeDetectionTest, RejectsInvalidArguments) {
    const cv::Mat gray = Square();
    EXPECT_THROW(GradientMagnitude(gray, -1.0), std::invalid_argument);
    EXPECT_THROW(GradientMagnitude(gray, 11.0), std::invalid_argument);
    EXPECT_THROW(GradientMagnitude(cv::Mat(4, 4, CV_32FC1), 0.0), std::invalid_argument);
    EXPECT_THROW(RenderEdgeMap(gray, EdgeMethod::Sobel, 0.0, 5.0, 5.0), std::invalid_argument);
    EXPECT_THROW(RenderEdgeMap(gray, EdgeMethod::Canny, 0.0, 6.0, 5.0), std::invalid_argument);
    EXPECT_THROW(RenderEdgeMap(gray, EdgeMethod::Canny, 0.0, -1.0, 5.0), std::invalid_argument);
    EXPECT_THROW(RenderEdgeMap(gray, EdgeMethod::Sobel, 0.0, 0.0, 5.0, -1), std::invalid_argument);
}

TEST(LivewireTest, FollowsTheStrongEdgeInsteadOfCrossingFlatAreas) {
    const cv::Mat gray = Square();
    const cv::Mat magnitude = GradientMagnitude(gray, 0.0);
    double largest = 0.0;
    cv::minMaxLoc(magnitude, nullptr, &largest);

    // From the left side of the square to its right side: the straight line crosses the flat inside, the edge goes around
    const auto path = LivewirePath(gray, cv::Point(10, 20), cv::Point(29, 20), 0.0);
    ASSERT_GE(path.size(), 2u);
    EXPECT_EQ(path.front(), (std::array<double, 2>{10.5, 20.5}));
    EXPECT_EQ(path.back(), (std::array<double, 2>{29.5, 20.5}));
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        // Walk the segment between two kept points pixel by pixel; the path is 8-connected with straight runs between them
        const int x0 = static_cast<int>(path[i][0]);
        const int y0 = static_cast<int>(path[i][1]);
        const int x1 = static_cast<int>(path[i + 1][0]);
        const int y1 = static_cast<int>(path[i + 1][1]);
        const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
        ASSERT_TRUE(std::abs(x1 - x0) == 0 || std::abs(y1 - y0) == 0 || std::abs(x1 - x0) == std::abs(y1 - y0));
        for (int s = 0; s <= steps; ++s) {
            const int x = x0 + (steps == 0 ? 0 : (x1 - x0) * s / steps);
            const int y = y0 + (steps == 0 ? 0 : (y1 - y0) * s / steps);
            EXPECT_GE(magnitude.at<float>(y, x), 0.25 * largest) << "pixel " << x << ", " << y;
        }
    }
}

TEST(LivewireTest, IsStraightOnAFlatImageAndRejectsInvalidPoints) {
    const cv::Mat flat(50, 50, CV_16UC1, cv::Scalar(500));
    EXPECT_EQ(LivewirePath(flat, cv::Point(5, 5), cv::Point(25, 5), 1.0), (std::vector<std::array<double, 2>>{{5.5, 5.5}, {25.5, 5.5}}));
    EXPECT_EQ(LivewirePath(flat, cv::Point(5, 5), cv::Point(5, 5), 1.0), (std::vector<std::array<double, 2>>{{5.5, 5.5}}));
    EXPECT_EQ(LivewirePath(flat, cv::Point(0, 0), cv::Point(49, 49), 0.0), (std::vector<std::array<double, 2>>{{0.5, 0.5}, {49.5, 49.5}}));

    EXPECT_THROW(LivewirePath(flat, cv::Point(-1, 0), cv::Point(5, 5), 1.0), std::invalid_argument);
    EXPECT_THROW(LivewirePath(flat, cv::Point(0, 0), cv::Point(50, 5), 1.0), std::invalid_argument);
    EXPECT_THROW(LivewirePath(flat, cv::Point(0, 0), cv::Point(5, 5), -0.5), std::invalid_argument);
    const cv::Mat wide(10, 2000, CV_8UC1, cv::Scalar(0));
    EXPECT_THROW(LivewirePath(wide, cv::Point(0, 0), cv::Point(1500, 0), 0.0), std::invalid_argument);
}
