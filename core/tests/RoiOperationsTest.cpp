#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <opencv2/core.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "roi/Roi.hpp"
#include "roi/RoiOperations.hpp"

using namespace glcm;

namespace {

// '#' = 255, anything else = 0
cv::Mat FromRows(const std::vector<std::string>& rows) {
    cv::Mat mask(static_cast<int>(rows.size()), static_cast<int>(rows[0].size()), CV_8UC1);
    for (int y = 0; y < mask.rows; ++y) {
        for (int x = 0; x < mask.cols; ++x) {
            mask.at<uchar>(y, x) = rows[y][x] == '#' ? 255 : 0;
        }
    }
    return mask;
}

cv::Mat Rasterize(const std::vector<std::array<double, 2>>& points, cv::Size size) {
    PolygonRoi polygon;
    polygon.points = points;
    return RasterizeMask(polygon, size);
}

int Differences(const cv::Mat& a, const cv::Mat& b) {
    return cv::countNonZero(a != b);
}

RoiShape Rectangle(double x, double y, double width, double height) {
    return RectangleRoi{x, y, width, height};
}

// Pixels whose centres lie within the radius of a segment, by brute force over the whole image
cv::Mat ReferenceStroke(const std::vector<std::array<double, 2>>& path, double radius, cv::Size size) {
    cv::Mat mask = cv::Mat::zeros(size, CV_8UC1);
    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            const double px = x + 0.5;
            const double py = y + 0.5;
            double best = INFINITY;
            for (size_t i = 0; i < path.size(); ++i) {
                const auto& a = path[i];
                const auto& b = path[std::min(i + 1, path.size() - 1)];
                const double dx = b[0] - a[0];
                const double dy = b[1] - a[1];
                const double length2 = dx * dx + dy * dy;
                double t = length2 > 0 ? ((px - a[0]) * dx + (py - a[1]) * dy) / length2 : 0.0;
                t = std::min(1.0, std::max(0.0, t));
                best = std::min(best, std::hypot(px - (a[0] + t * dx), py - (a[1] + t * dy)));
            }
            if (best <= radius) {
                mask.at<uchar>(y, x) = 255;
            }
        }
    }
    return mask;
}

} // namespace

TEST(RoiOperationsTest, MaskOutlineReproducesMasksWithHolesIslandsAndSeveralParts) {
    const std::vector<std::vector<std::string>> cases = {
        {"#"},
        {"....", ".##.", ".##.", "...."},
        // A ring with an island in its hole, a diagonal hole, and a separate part
        {
            "############...",
            "#..........#...",
            "#.####.....#.#.",
            "#.#..#.....#...",
            "#.####..#..#.##",
            "#......#...#.##",
            "############...",
        },
        // Diagonal neighbours in the parts and in the background
        {"#.#.#", ".#.#.", "#.#.#", ".#.#.", "#.#.#"},
        {"##..", "##..", "..##", "..##"},
        {".....", ".###.", ".#.#.", ".###.", "....."},
    };
    for (size_t i = 0; i < cases.size(); ++i) {
        const cv::Mat mask = FromRows(cases[i]);
        const auto outline = MaskOutline(mask);
        EXPECT_EQ(Differences(Rasterize(outline, mask.size()), mask), 0) << "case " << i;
    }

    // Masks with many holes and parts
    cv::Mat pattern(40, 50, CV_8UC1);
    for (int y = 0; y < pattern.rows; ++y) {
        for (int x = 0; x < pattern.cols; ++x) {
            pattern.at<uchar>(y, x) = static_cast<uchar>((x * 37 + y * 101 + (x * y) % 7 * 13) % 256);
        }
    }
    for (int threshold : {40, 100, 128, 200}) {
        const cv::Mat mask = pattern > threshold;
        EXPECT_EQ(Differences(Rasterize(MaskOutline(mask), mask.size()), mask), 0) << "threshold " << threshold;
    }
}

TEST(RoiOperationsTest, MaskOutlineOfOneRectangleIsItsCornersAndHonoursTheOffset) {
    const cv::Mat mask = FromRows({"###", "###"});
    EXPECT_EQ(MaskOutline(mask, cv::Point(4, 7)), (std::vector<std::array<double, 2>>{{4, 7}, {7, 7}, {7, 9}, {4, 9}}));
    EXPECT_TRUE(MaskOutline(cv::Mat::zeros(3, 3, CV_8UC1)).empty());
    EXPECT_THROW(MaskOutline(cv::Mat::zeros(3, 3, CV_32FC1)), std::invalid_argument);

    cv::Mat image = cv::Mat::zeros(20, 20, CV_8UC1);
    const cv::Mat ring = FromRows({"###", "#.#", "###"});
    ring.copyTo(image(cv::Rect(12, 5, 3, 3)));
    EXPECT_EQ(Differences(Rasterize(MaskOutline(ring, cv::Point(12, 5)), image.size()), image), 0);
}

TEST(RoiOperationsTest, UnionAndSubtractMatchTheRasterizedShapes) {
    const cv::Size size(40, 30);
    const RoiShape a = Rectangle(2, 3, 20, 15);
    const RoiShape b = EllipseRoi{20, 15, 9, 6, 30};
    const RoiShape inner = Rectangle(8, 7, 6, 5);
    const RoiShape far = Rectangle(30, 22, 5, 5);

    const auto check = [&size](const OperationResult& result, const cv::Mat& expected, const char* label) {
        EXPECT_EQ(result.pixel_count, cv::countNonZero(expected)) << label;
        EXPECT_EQ(Differences(Rasterize(result.polygon.points, size), expected), 0) << label;
        EXPECT_EQ(result.box, MaskBoundingBox(expected)) << label;
    };
    check(CombineShapes({a, b}, RoiOperation::Union, size), RasterizeMask(a, size) | RasterizeMask(b, size), "overlapping union");
    check(CombineShapes({a, far}, RoiOperation::Union, size), RasterizeMask(a, size) | RasterizeMask(far, size), "separate parts");
    check(CombineShapes({a, b}, RoiOperation::Subtract, size), RasterizeMask(a, size) & ~RasterizeMask(b, size), "subtract");
    check(CombineShapes({a, inner, far}, RoiOperation::Subtract, size), RasterizeMask(a, size) & ~RasterizeMask(inner, size), "hole");

    const OperationResult hole = CombineShapes({a, inner}, RoiOperation::Subtract, size);
    EXPECT_EQ(hole.pixel_count, 20 * 15 - 6 * 5);
    EXPECT_GT(hole.polygon.points.size(), 8u);

    const OperationResult nothing = CombineShapes({inner, a}, RoiOperation::Subtract, size);
    EXPECT_EQ(nothing.pixel_count, 0);
    EXPECT_TRUE(nothing.polygon.points.empty());
    EXPECT_EQ(CombineShapes({Rectangle(-10, -10, 5, 5), Rectangle(50, 50, 5, 5)}, RoiOperation::Union, size).pixel_count, 0);
    EXPECT_THROW(CombineShapes({}, RoiOperation::Union, size), std::invalid_argument);
}

TEST(RoiOperationsTest, BrushStrokesPaintAndEraseTheCoveredPixels) {
    const cv::Size size(30, 25);
    const std::vector<std::array<double, 2>> dot{{10.3, 7.8}};
    const std::vector<std::array<double, 2>> line{{3.0, 20.0}, {18.5, 12.25}, {26.0, 21.0}};

    const OperationResult disc = PaintStroke(std::nullopt, dot, 3.0, false, size);
    const cv::Mat expected_disc = ReferenceStroke(dot, 3.0, size);
    EXPECT_EQ(disc.pixel_count, cv::countNonZero(expected_disc));
    EXPECT_EQ(Differences(Rasterize(disc.polygon.points, size), expected_disc), 0);

    const RoiShape base = Rectangle(5, 5, 15, 15);
    const OperationResult painted = PaintStroke(base, line, 2.5, false, size);
    const cv::Mat expected_painted = RasterizeMask(base, size) | ReferenceStroke(line, 2.5, size);
    EXPECT_EQ(Differences(Rasterize(painted.polygon.points, size), expected_painted), 0);

    // Erasing a band through the rectangle splits it into two parts
    const std::vector<std::array<double, 2>> band{{12.0, 0.0}, {12.0, 30.0}};
    const OperationResult split = PaintStroke(base, band, 1.5, true, size);
    const cv::Mat expected_split = RasterizeMask(base, size) & ~ReferenceStroke(band, 1.5, size);
    EXPECT_EQ(split.pixel_count, cv::countNonZero(expected_split));
    EXPECT_EQ(Differences(Rasterize(split.polygon.points, size), expected_split), 0);

    // A stroke over the image edge is clipped; erasing without a shape or everything leaves nothing
    const std::vector<std::array<double, 2>> corner{{0.0, 0.0}};
    EXPECT_EQ(Differences(
                  Rasterize(PaintStroke(std::nullopt, corner, 4.0, false, size).polygon.points, size), ReferenceStroke(corner, 4.0, size)),
        0);
    EXPECT_EQ(PaintStroke(std::nullopt, dot, 3.0, true, size).pixel_count, 0);
    EXPECT_EQ(PaintStroke(Rectangle(9, 7, 2, 2), dot, 5.0, true, size).pixel_count, 0);
    EXPECT_EQ(PaintStroke(std::nullopt, {{-50.0, -50.0}}, 2.0, false, size).pixel_count, 0);

    EXPECT_THROW(PaintStroke(std::nullopt, {}, 2.0, false, size), std::invalid_argument);
    EXPECT_THROW(PaintStroke(std::nullopt, dot, 0.0, false, size), std::invalid_argument);
    EXPECT_THROW(PaintStroke(std::nullopt, dot, NAN, false, size), std::invalid_argument);
    EXPECT_THROW(PaintStroke(std::nullopt, {{NAN, 1.0}}, 2.0, false, size), std::invalid_argument);
}
