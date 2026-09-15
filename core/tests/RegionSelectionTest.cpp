#include <gtest/gtest.h>

#include <array>
#include <deque>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "roi/RegionSelection.hpp"
#include "roi/Roi.hpp"

using namespace glcm;

namespace {

// '#' = 255, anything else = 0
cv::Mat FromRows(const std::vector<std::string>& rows) {
    cv::Mat image(static_cast<int>(rows.size()), static_cast<int>(rows[0].size()), CV_8UC1);
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
            image.at<uchar>(y, x) = rows[y][x] == '#' ? 255 : 0;
        }
    }
    return image;
}

cv::Mat Pattern8(int rows, int cols) {
    cv::Mat image(rows, cols, CV_8UC1);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            image.at<uchar>(y, x) = static_cast<uchar>((x * 37 + y * 101 + (x * y) % 7 * 13) % 256);
        }
    }
    return image;
}

cv::Mat OutlineMask(const SelectedRegion& region, cv::Size size) {
    PolygonRoi polygon;
    polygon.points = region.outline;
    return RasterizeMask(polygon, size);
}

// Reference for the thresholded pixels with holes filled: background reached from the border through 4-connected
// background pixels stays outside, everything else is inside
cv::Mat ReferenceFilledMask(const cv::Mat& gray, int low, int high) {
    cv::Mat outside = cv::Mat::zeros(gray.size(), CV_8UC1);
    std::deque<std::array<int, 2>> queue;
    const auto background = [&](int x, int y) {
        const int value = gray.at<uchar>(y, x);
        return value < low || value > high;
    };
    const auto visit = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < gray.cols && y < gray.rows && outside.at<uchar>(y, x) == 0 && background(x, y)) {
            outside.at<uchar>(y, x) = 255;
            queue.push_back({x, y});
        }
    };
    for (int x = 0; x < gray.cols; ++x) {
        visit(x, 0);
        visit(x, gray.rows - 1);
    }
    for (int y = 0; y < gray.rows; ++y) {
        visit(0, y);
        visit(gray.cols - 1, y);
    }
    while (!queue.empty()) {
        const auto [x, y] = queue.front();
        queue.pop_front();
        visit(x + 1, y);
        visit(x - 1, y);
        visit(x, y + 1);
        visit(x, y - 1);
    }
    cv::Mat inside = outside == 0;
    return inside;
}

// Consecutive vertices differ along one axis, alternating between horizontal and vertical edges
void ExpectAxisAlignedTurns(const SelectedRegion& region) {
    const auto& points = region.outline;
    ASSERT_GE(points.size(), 4u);
    ASSERT_EQ(points.size() % 2, 0u);
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& a = points[i];
        const auto& b = points[(i + 1) % points.size()];
        const bool horizontal = a[1] == b[1] && a[0] != b[0];
        const bool vertical = a[0] == b[0] && a[1] != b[1];
        EXPECT_TRUE(i % 2 == 0 ? horizontal : vertical) << "edge " << i;
    }
}

} // namespace

TEST(RegionSelectionTest, OutlinesRasterizeToTheThresholdedPixelsWithHolesFilled) {
    const cv::Mat gray = Pattern8(30, 40);
    for (const auto& [low, high] : std::vector<std::array<int, 2>>{{60, 140}, {0, 40}, {200, 255}, {0, 255}}) {
        const ThresholdSelection selection = SelectThresholdRegions(gray, low, high, 1, 100000);
        ASSERT_EQ(selection.total, static_cast<int>(selection.regions.size()));
        cv::Mat all = cv::Mat::zeros(gray.size(), CV_8UC1);
        int pixels = 0;
        for (size_t i = 0; i < selection.regions.size(); ++i) {
            const SelectedRegion& region = selection.regions[i];
            const cv::Mat mask = OutlineMask(region, gray.size());
            EXPECT_EQ(CountMaskPixels(mask), region.pixel_count) << low << "-" << high << " region " << i;
            EXPECT_EQ(MaskBoundingBox(mask), region.box);
            if (i > 0) {
                EXPECT_LE(region.pixel_count, selection.regions[i - 1].pixel_count);
            }
            ExpectAxisAlignedTurns(region);
            all |= mask;
            pixels += region.pixel_count;
        }
        // The regions do not overlap and together cover exactly the thresholded pixels with holes filled
        EXPECT_EQ(CountMaskPixels(all), pixels);
        EXPECT_EQ(cv::countNonZero(all != ReferenceFilledMask(gray, low, high)), 0) << low << "-" << high;
    }
}

TEST(RegionSelectionTest, JoinsDiagonalNeighboursAndFillsHoles) {
    const cv::Mat gray = FromRows({
        "..........",
        ".####.....",
        ".##.#.....",
        ".#..#.#...",
        ".####..#..",
        "........#.",
        "..........",
    });
    const ThresholdSelection selection = SelectThresholdRegions(gray, 255, 255, 1, 10);
    ASSERT_EQ(selection.total, 2);
    // The ring with its hole, including the pixel in the hole, then the diagonal chain
    EXPECT_EQ(selection.regions[0].pixel_count, 16);
    EXPECT_EQ(selection.regions[0].box, cv::Rect(1, 1, 4, 4));
    EXPECT_EQ(selection.regions[0].outline, (std::vector<std::array<double, 2>>{{1, 1}, {5, 1}, {5, 5}, {1, 5}}));
    EXPECT_EQ(selection.regions[1].pixel_count, 3);
    EXPECT_EQ(CountMaskPixels(OutlineMask(selection.regions[1], gray.size())), 3);
    EXPECT_EQ(selection.regions[1].outline.size(), 12u);

    EXPECT_EQ(SelectThresholdRegions(gray, 255, 255, 4, 10).total, 1);
    const ThresholdSelection first = SelectThresholdRegions(gray, 255, 255, 1, 1);
    EXPECT_EQ(first.total, 2);
    ASSERT_EQ(first.regions.size(), 1u);
    EXPECT_EQ(first.regions[0].pixel_count, 16);
    EXPECT_EQ(SelectThresholdRegions(gray, 255, 255, 1, 0).regions.size(), 0u);
    EXPECT_EQ(SelectThresholdRegions(gray, 1, 254, 1, 10).total, 0);
}

TEST(RegionSelectionTest, OutlinesOnePixelClockwise) {
    const cv::Mat gray = FromRows({"...", ".#.", "..."});
    const ThresholdSelection selection = SelectThresholdRegions(gray, 200, 255, 1, 10);
    ASSERT_EQ(selection.regions.size(), 1u);
    EXPECT_EQ(selection.regions[0].outline, (std::vector<std::array<double, 2>>{{1, 1}, {2, 1}, {2, 2}, {1, 2}}));
    // A region touching the image border
    const ThresholdSelection whole = SelectThresholdRegions(gray, 0, 255, 1, 10);
    ASSERT_EQ(whole.regions.size(), 1u);
    EXPECT_EQ(whole.regions[0].outline, (std::vector<std::array<double, 2>>{{0, 0}, {3, 0}, {3, 3}, {0, 3}}));
}

TEST(RegionSelectionTest, WandSelectsConnectedPixelsWithinTheTolerance) {
    cv::Mat gray(10, 20, CV_16UC1);
    for (int y = 0; y < gray.rows; ++y) {
        for (int x = 0; x < gray.cols; ++x) {
            gray.at<uint16_t>(y, x) = static_cast<uint16_t>(x < 10 ? 1000 + x % 3 : 5000);
        }
    }
    const auto left = SelectWandRegion(gray, 2, 2, 5);
    ASSERT_TRUE(left.has_value());
    EXPECT_EQ(left->pixel_count, 100);
    EXPECT_EQ(left->box, cv::Rect(0, 0, 10, 10));
    EXPECT_EQ(left->outline, (std::vector<std::array<double, 2>>{{0, 0}, {10, 0}, {10, 10}, {0, 10}}));

    // Tolerance 0 at a value that occurs in every third column only: that column
    const auto column = SelectWandRegion(gray, 2, 7, 0);
    ASSERT_TRUE(column.has_value());
    EXPECT_EQ(column->pixel_count, 10);
    EXPECT_EQ(column->box, cv::Rect(2, 0, 1, 10));

    EXPECT_EQ(SelectWandRegion(gray, 15, 0, 3999)->pixel_count, 100);
    EXPECT_EQ(SelectWandRegion(gray, 15, 0, 4000)->pixel_count, 200);
    EXPECT_FALSE(SelectWandRegion(gray, 20, 0, 5).has_value());
    EXPECT_FALSE(SelectWandRegion(gray, -1, 0, 5).has_value());
}

TEST(RegionSelectionTest, WandFillsHolesAndMatchesItsOutline) {
    const cv::Mat gray = FromRows({
        "..........",
        ".####.....",
        ".##.#.....",
        ".#..#.#...",
        ".####..#..",
        "........#.",
        "..........",
    });
    const auto ring = SelectWandRegion(gray, 4, 4, 0);
    ASSERT_TRUE(ring.has_value());
    EXPECT_EQ(ring->pixel_count, 16);
    EXPECT_EQ(CountMaskPixels(OutlineMask(*ring, gray.size())), 16);

    const auto chain = SelectWandRegion(gray, 8, 5, 0);
    ASSERT_TRUE(chain.has_value());
    EXPECT_EQ(chain->pixel_count, 3);
    EXPECT_EQ(chain->outline, SelectThresholdRegions(gray, 255, 255, 1, 10).regions[1].outline);

    // The background around the shapes: the shapes lie in its holes and are filled in, so it covers the whole image
    const auto background = SelectWandRegion(gray, 0, 0, 0);
    ASSERT_TRUE(background.has_value());
    EXPECT_EQ(background->pixel_count, 70);
    EXPECT_EQ(background->outline, (std::vector<std::array<double, 2>>{{0, 0}, {10, 0}, {10, 7}, {0, 7}}));
}

TEST(RegionSelectionTest, RejectsInvalidArguments) {
    const cv::Mat gray = Pattern8(5, 5);
    EXPECT_THROW(SelectThresholdRegions(gray, 10, 9, 1, 1), std::invalid_argument);
    EXPECT_THROW(SelectThresholdRegions(gray, 0, 255, 0, 1), std::invalid_argument);
    EXPECT_THROW(SelectThresholdRegions(gray, 0, 255, 1, -1), std::invalid_argument);
    EXPECT_THROW(SelectWandRegion(gray, 0, 0, -1), std::invalid_argument);
    EXPECT_THROW(SelectThresholdRegions(cv::Mat(5, 5, CV_32FC1, cv::Scalar(0)), 0, 1, 1, 1), std::invalid_argument);
    EXPECT_THROW(SelectWandRegion(cv::Mat(), 0, 0, 1), std::invalid_argument);
}
