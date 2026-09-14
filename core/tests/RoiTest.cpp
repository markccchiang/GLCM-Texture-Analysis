#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <opencv2/core.hpp>
#include <random>
#include <stdexcept>
#include <vector>

#include "roi/Roi.hpp"

using glcm::CountMaskPixels;
using glcm::EllipseRoi;
using glcm::MaskBoundingBox;
using glcm::PolygonRoi;
using glcm::RasterizeMask;
using glcm::RectangleRoi;

namespace {

using Points = std::vector<std::array<double, 2>>;

bool MasksEqual(const cv::Mat& a, const cv::Mat& b) {
    if (a.size() != b.size() || a.type() != b.type()) {
        return false;
    }
    cv::Mat difference = a != b;
    return cv::countNonZero(difference) == 0;
}

PolygonRoi Polygon(const Points& points) {
    PolygonRoi polygon;
    polygon.points = points;
    return polygon;
}

// Independent even-odd point-in-polygon test (ray casting, W. R. Franklin's pnpoly) at every pixel centre
cv::Mat BruteForcePolygonMask(const Points& points, cv::Size size) {
    cv::Mat mask = cv::Mat::zeros(size, CV_8UC1);
    const size_t n = points.size();
    for (int row = 0; row < size.height; ++row) {
        for (int col = 0; col < size.width; ++col) {
            const double x = col + 0.5;
            const double y = row + 0.5;
            bool inside = false;
            for (size_t i = 0, j = n - 1; i < n; j = i++) {
                const double xi = points[i][0];
                const double yi = points[i][1];
                const double xj = points[j][0];
                const double yj = points[j][1];
                if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / (yj - yi) + xi)) {
                    inside = !inside;
                }
            }
            if (inside) {
                mask.at<uchar>(row, col) = 255;
            }
        }
    }
    return mask;
}

double ShoelaceArea(const Points& points) {
    double twice_area = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& a = points[i];
        const auto& b = points[(i + 1) % points.size()];
        twice_area += a[0] * b[1] - b[0] * a[1];
    }
    return std::fabs(twice_area) / 2.0;
}

double Perimeter(const Points& points) {
    double perimeter = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& a = points[i];
        const auto& b = points[(i + 1) % points.size()];
        perimeter += std::hypot(b[0] - a[0], b[1] - a[1]);
    }
    return perimeter;
}

} // namespace

TEST(RoiTest, RectangleCoversPixelCentres) {
    const cv::Mat mask = RasterizeMask(RectangleRoi{2.0, 3.0, 4.0, 5.0}, cv::Size(20, 20));

    EXPECT_EQ(mask.type(), CV_8UC1);
    EXPECT_EQ(CountMaskPixels(mask), 20);
    EXPECT_EQ(MaskBoundingBox(mask), cv::Rect(2, 3, 4, 5));
    EXPECT_EQ(mask.at<uchar>(3, 2), 255);
    EXPECT_EQ(mask.at<uchar>(8, 2), 0);
}

TEST(RoiTest, RectangleUsesHalfOpenPixelCentreRule) {
    // Centre 3.5 lies in [2.6, 3.6); centre 2.5 does not
    EXPECT_EQ(MaskBoundingBox(RasterizeMask(RectangleRoi{2.6, 0.0, 1.0, 1.0}, cv::Size(10, 3))), cv::Rect(3, 0, 1, 1));
    // Centre 2.5 lies in [2.5, 3.5); centre 3.5 is on the open right edge
    EXPECT_EQ(MaskBoundingBox(RasterizeMask(RectangleRoi{2.5, 0.0, 1.0, 1.0}, cv::Size(10, 3))), cv::Rect(2, 0, 1, 1));
}

TEST(RoiTest, RectangleWithNegativeSizeIsNormalized) {
    const cv::Size size(20, 20);
    EXPECT_TRUE(MasksEqual(RasterizeMask(RectangleRoi{6.0, 8.0, -4.0, -5.0}, size), RasterizeMask(RectangleRoi{2.0, 3.0, 4.0, 5.0}, size)));
}

TEST(RoiTest, ShapesAreClippedToTheImage) {
    const cv::Size size(8, 8);
    EXPECT_EQ(CountMaskPixels(RasterizeMask(RectangleRoi{-5.0, -5.0, 10.0, 10.0}, size)), 25);
    EXPECT_EQ(CountMaskPixels(RasterizeMask(RectangleRoi{20.0, 20.0, 5.0, 5.0}, size)), 0);

    const cv::Mat ellipse = RasterizeMask(EllipseRoi{0.0, 0.0, 6.0, 6.0, 0.0}, size);
    EXPECT_GT(CountMaskPixels(ellipse), 0);
    EXPECT_EQ(MaskBoundingBox(ellipse).tl(), cv::Point(0, 0));

    const Points outside_left = {{-30.0, 1.0}, {4.0, 1.0}, {4.0, 5.0}, {-30.0, 5.0}};
    EXPECT_TRUE(MasksEqual(RasterizeMask(Polygon(outside_left), size), BruteForcePolygonMask(outside_left, size)));
}

TEST(RoiTest, AxisAlignedSquarePolygonEqualsRectangle) {
    const cv::Size size(20, 20);
    const PolygonRoi square = Polygon({{2.0, 3.0}, {6.0, 3.0}, {6.0, 8.0}, {2.0, 8.0}});
    EXPECT_TRUE(MasksEqual(RasterizeMask(square, size), RasterizeMask(RectangleRoi{2.0, 3.0, 4.0, 5.0}, size)));
}

TEST(RoiTest, PolygonsMatchBruteForceEvenOddTest) {
    const cv::Size size(40, 40);
    // Vertices are off the pixel-centre grid, so no centre lies exactly on an edge
    const Points concave = {{3.3, 2.2}, {30.7, 4.1}, {18.2, 14.6}, {33.9, 31.3}, {5.1, 36.8}, {12.4, 19.9}};
    const Points bow_tie = {{2.3, 2.1}, {37.6, 36.2}, {37.4, 2.7}, {2.9, 35.8}};

    for (const Points& points : {concave, bow_tie}) {
        const cv::Mat mask = RasterizeMask(Polygon(points), size);
        EXPECT_TRUE(MasksEqual(mask, BruteForcePolygonMask(points, size)));
    }

    // The pixel count of a simple polygon is close to its area (the error is bounded by the boundary length)
    const int count = CountMaskPixels(RasterizeMask(Polygon(concave), size));
    EXPECT_NEAR(count, ShoelaceArea(concave), Perimeter(concave));
}

TEST(RoiTest, CircleAreaIsCloseToPiRSquared) {
    const cv::Mat mask = RasterizeMask(EllipseRoi{50.0, 50.0, 20.0, 20.0, 0.0}, cv::Size(100, 100));
    EXPECT_NEAR(CountMaskPixels(mask), CV_PI * 20.0 * 20.0, 40.0);
    // Centres 30.5 ... 69.5 are within 20 of 50 on the middle row and column
    EXPECT_EQ(MaskBoundingBox(mask), cv::Rect(30, 30, 40, 40));
}

TEST(RoiTest, EllipseRotationMatchesSwappedAxes) {
    const cv::Size size(60, 60);
    const cv::Mat wide = RasterizeMask(EllipseRoi{30.0, 30.0, 20.0, 8.0, 0.0}, size);
    const cv::Mat tall = RasterizeMask(EllipseRoi{30.0, 30.0, 8.0, 20.0, 0.0}, size);

    EXPECT_TRUE(MasksEqual(RasterizeMask(EllipseRoi{30.0, 30.0, 20.0, 8.0, 90.0}, size), tall));
    EXPECT_TRUE(MasksEqual(RasterizeMask(EllipseRoi{30.0, 30.0, 20.0, 8.0, 180.0}, size), wide));
    EXPECT_TRUE(MasksEqual(RasterizeMask(EllipseRoi{30.0, 30.0, 20.0, 8.0, -90.0}, size), tall));
    EXPECT_FALSE(MasksEqual(RasterizeMask(EllipseRoi{30.0, 30.0, 20.0, 8.0, 30.0}, size), wide));
}

TEST(RoiTest, DegenerateShapesGiveEmptyMasks) {
    const cv::Size size(10, 10);
    EXPECT_EQ(CountMaskPixels(RasterizeMask(RectangleRoi{2.0, 2.0, 0.0, 5.0}, size)), 0);
    EXPECT_EQ(CountMaskPixels(RasterizeMask(EllipseRoi{5.0, 5.0, 0.0, 3.0, 0.0}, size)), 0);
    EXPECT_EQ(CountMaskPixels(RasterizeMask(Polygon({{1.0, 1.0}, {8.0, 8.0}}), size)), 0);
    EXPECT_EQ(MaskBoundingBox(cv::Mat::zeros(5, 5, CV_8UC1)), cv::Rect());
}

TEST(RoiTest, InvalidInputsThrow) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const cv::Size size(10, 10);

    EXPECT_THROW(RasterizeMask(RectangleRoi{nan, 0.0, 1.0, 1.0}, size), std::invalid_argument);
    EXPECT_THROW(RasterizeMask(EllipseRoi{5.0, 5.0, inf, 3.0, 0.0}, size), std::invalid_argument);
    EXPECT_THROW(RasterizeMask(Polygon({{1.0, 1.0}, {8.0, nan}, {2.0, 7.0}}), size), std::invalid_argument);
    EXPECT_THROW(RasterizeMask(RectangleRoi{0.0, 0.0, 1.0, 1.0}, cv::Size(0, 10)), std::invalid_argument);
    EXPECT_THROW(MaskBoundingBox(cv::Mat::zeros(5, 5, CV_16UC1)), std::invalid_argument);
    EXPECT_THROW(CountMaskPixels(cv::Mat::zeros(5, 5, CV_32FC1)), std::invalid_argument);
}

// Finite coordinates far outside the image must neither produce NaN (undefined when converted to int) nor wrong masks
TEST(RoiTest, ExtremeCoordinatesGiveCorrectMasks) {
    const cv::Size size(10, 10);
    const double max = std::numeric_limits<double>::max();

    // The edge from (-1e308, 0.5) to (1e308, 9.5) crosses the image at y = 5, so the polygon covers the rows below it.
    // The direct crossing formula would overflow: (y - 0.5) * (1e308 - -1e308) is 0 * inf at row 0 and inf below.
    const cv::Mat above_edge = RasterizeMask(Polygon({{-1e308, 0.5}, {1e308, 9.5}, {1e308, 20.0}, {-1e308, 20.0}}), size);
    EXPECT_TRUE(MasksEqual(above_edge, RasterizeMask(RectangleRoi{0.0, 5.0, 10.0, 5.0}, size)));
    const cv::Mat full_band = RasterizeMask(Polygon({{-max, 2.0}, {max, 2.0}, {max, 6.0}, {-max, 6.0}}), size);
    EXPECT_TRUE(MasksEqual(full_band, RasterizeMask(RectangleRoi{0.0, 2.0, 10.0, 4.0}, size)));

    // A radius whose square overflows: the bounding box computation would take sqrt(inf * 0)
    const cv::Mat wide_ellipse = RasterizeMask(EllipseRoi{5.0, 5.0, 1e200, 3.0, 0.0}, size);
    EXPECT_TRUE(MasksEqual(wide_ellipse, RasterizeMask(RectangleRoi{0.0, 2.0, 10.0, 6.0}, size)));
    const cv::Mat tall_ellipse = RasterizeMask(EllipseRoi{5.0, 5.0, 3.0, 1e200, 0.0}, size);
    EXPECT_TRUE(MasksEqual(tall_ellipse, RasterizeMask(RectangleRoi{2.0, 0.0, 6.0, 10.0}, size)));
    // A radius whose square underflows to 0 contains no pixel centre
    EXPECT_EQ(CountMaskPixels(RasterizeMask(EllipseRoi{5.0, 5.0, 1e-200, 3.0, 0.0}, size)), 0);

    // Rectangles whose far corner is computed from extreme values: ending at the origin, and covering the image
    EXPECT_EQ(CountMaskPixels(RasterizeMask(RectangleRoi{-max, -max, max, max}, size)), 0);
    EXPECT_EQ(CountMaskPixels(RasterizeMask(RectangleRoi{-1e308, -1e308, max, max}, size)), 100);
}

// RasterizeCroppedMask must give exactly the full mask, placed at its box
TEST(RoiTest, CroppedMasksEqualFullMasks) {
    const cv::Size size(64, 48);
    const double max = std::numeric_limits<double>::max();
    std::vector<glcm::RoiShape> shapes = {RectangleRoi{10.2, 5.5, 20.0, 9.7}, RectangleRoi{30.0, 40.0, -12.5, -30.0},
        RectangleRoi{-5.0, -5.0, 12.0, 12.0}, RectangleRoi{60.0, 44.0, 10.0, 10.0}, RectangleRoi{100.0, 100.0, 5.0, 5.0},
        RectangleRoi{3.2, 3.2, 0.1, 0.1}, RectangleRoi{-1e308, -1e308, max, max}, EllipseRoi{20.0, 20.0, 9.5, 4.25, 33.0},
        EllipseRoi{62.0, 2.0, 15.0, 6.0, -70.0}, EllipseRoi{5.0, 5.0, 1e200, 3.0, 0.0}, EllipseRoi{30.0, 30.0, 0.0, 5.0, 0.0},
        EllipseRoi{-50.0, -50.0, 10.0, 10.0, 0.0}, EllipseRoi{32.5, 24.5, 0.4, 0.4, 0.0}, Polygon({{5.0, 5.0}, {40.0, 8.0}, {20.0, 30.0}}),
        Polygon({{10.0, 10.0}, {50.0, 40.0}, {50.0, 10.0}, {10.0, 40.0}}), Polygon({{-20.0, 10.0}, {90.0, 12.5}, {30.0, 70.0}}),
        Polygon({{-1e308, 0.5}, {1e308, 9.5}, {1e308, 20.0}, {-1e308, 20.0}}), Polygon({{1.0, 1.0}, {2.0, 2.0}}),
        Polygon({{63.9, 47.9}, {70.0, 47.95}, {64.0, 60.0}})};

    std::mt19937 random(12345);
    std::uniform_real_distribution<double> coordinate(-10.0, 74.0);
    for (int i = 0; i < 200; ++i) {
        Points points;
        for (int k = 0; k < 3 + i % 6; ++k) {
            points.push_back({coordinate(random), coordinate(random)});
        }
        shapes.push_back(Polygon(points));
        shapes.push_back(EllipseRoi{coordinate(random), coordinate(random), std::abs(coordinate(random)) / 3.0,
            std::abs(coordinate(random)) / 4.0, coordinate(random) * 5.0});
        shapes.push_back(RectangleRoi{coordinate(random), coordinate(random), coordinate(random) / 2.0, coordinate(random) / 2.0});
    }

    for (size_t i = 0; i < shapes.size(); ++i) {
        SCOPED_TRACE("shape " + std::to_string(i));
        const cv::Mat full = RasterizeMask(shapes[i], size);
        const glcm::CroppedMask cropped = glcm::RasterizeCroppedMask(shapes[i], size);
        ASSERT_EQ(cropped.box.size(), cropped.mask.size());
        ASSERT_EQ(cropped.box & cv::Rect(cv::Point(0, 0), size), cropped.box);
        cv::Mat placed = cv::Mat::zeros(size, CV_8UC1);
        if (!cropped.mask.empty()) {
            cropped.mask.copyTo(placed(cropped.box));
        }
        EXPECT_TRUE(MasksEqual(placed, full));
        EXPECT_EQ(CountMaskPixels(cropped.mask), CountMaskPixels(full));
    }
}

TEST(RoiTest, CroppedMasksStaySmallAndValidateInput) {
    // Small shapes on a huge image get small boxes, and no full-size mask is allocated
    const cv::Size huge(20000, 20000);
    EXPECT_EQ(glcm::RasterizeCroppedMask(RectangleRoi{100.0, 200.0, 10.0, 5.0}, huge).box, cv::Rect(100, 200, 10, 5));
    EXPECT_LE(glcm::RasterizeCroppedMask(EllipseRoi{5000.0, 5000.0, 6.0, 4.0, 30.0}, huge).box.area(), 16 * 16);
    EXPECT_LE(glcm::RasterizeCroppedMask(Polygon({{10.0, 10.0}, {22.0, 12.0}, {15.0, 21.0}}), huge).box.area(), 16 * 14);

    const glcm::CroppedMask outside = glcm::RasterizeCroppedMask(RectangleRoi{-50.0, -50.0, 10.0, 10.0}, cv::Size(10, 10));
    EXPECT_TRUE(outside.box.empty());
    EXPECT_TRUE(outside.mask.empty());
    EXPECT_EQ(CountMaskPixels(outside.mask), 0);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(glcm::RasterizeCroppedMask(Polygon({{1.0, 1.0}, {nan, 2.0}, {3.0, 1.0}}), cv::Size(10, 10)), std::invalid_argument);
    EXPECT_THROW(glcm::RasterizeCroppedMask(EllipseRoi{nan, 1.0, 1.0, 1.0, 0.0}, cv::Size(10, 10)), std::invalid_argument);
    EXPECT_THROW(glcm::RasterizeCroppedMask(RectangleRoi{0.0, 0.0, 1.0, 1.0}, cv::Size(0, 5)), std::invalid_argument);
}
