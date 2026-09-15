#include "roi/RegionSelection.hpp"

#include <algorithm>
#include <cstdint>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace glcm {

namespace {

const uchar INSIDE = 255;
const uchar FLOODED = 128;

void CheckImage(const cv::Mat& gray) {
    if (gray.empty() || gray.channels() != 1 || (gray.depth() != CV_8U && gray.depth() != CV_16U)) {
        throw std::invalid_argument("The image must be an 8- or 16-bit single-channel image");
    }
}

// 255 where low <= value <= high
cv::Mat RangeMask(const cv::Mat& gray, int64_t low, int64_t high) {
    cv::Mat mask;
    cv::inRange(gray, cv::Scalar(static_cast<double>(low)), cv::Scalar(static_cast<double>(high)), mask);
    return mask;
}

// The mask with its holes set to 255: background pixels that are not 4-connected to the border. 4-connected background
// is the counterpart of 8-connected regions, so a diagonal gap does not let the background through.
cv::Mat FillHoles(const cv::Mat& mask) {
    cv::Mat padded;
    cv::copyMakeBorder(mask, padded, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(0));
    cv::floodFill(padded, cv::Point(0, 0), cv::Scalar(FLOODED), nullptr, cv::Scalar(0), cv::Scalar(0), 4);
    cv::Mat filled = padded(cv::Rect(1, 1, mask.cols, mask.rows)) != FLOODED;
    return filled;
}

// Outline along the pixel edges of the region containing pixel (start_x, start_y), which must be the region's first
// pixel in raster order. `inside(x, y)` tells whether a pixel belongs to the region (false outside the image). The region
// must have no holes. The walk keeps the region on its right and prefers turning left, which joins parts that touch only
// at a corner.
template <typename Inside>
std::vector<std::array<double, 2>> TraceOutline(int start_x, int start_y, Inside inside) {
    // Directions clockwise on screen (y down): right, down, left, up
    static const int STEP_X[4] = {1, 0, -1, 0};
    static const int STEP_Y[4] = {0, 1, 0, -1};
    // Offsets from a vertex to the pixel ahead on the left and ahead on the right of each direction
    static const int LEFT_X[4] = {0, 0, -1, -1};
    static const int LEFT_Y[4] = {-1, 0, 0, -1};
    static const int RIGHT_X[4] = {0, -1, -1, 0};
    static const int RIGHT_Y[4] = {0, 0, -1, -1};

    std::vector<std::array<double, 2>> outline{{static_cast<double>(start_x), static_cast<double>(start_y)}};
    int x = start_x;
    int y = start_y;
    int direction = 0; // along the top edge of the first pixel, whose upper and left neighbours are outside
    do {
        x += STEP_X[direction];
        y += STEP_Y[direction];
        int next = (direction + 1) % 4;
        if (inside(x + LEFT_X[direction], y + LEFT_Y[direction])) {
            next = (direction + 3) % 4;
        } else if (inside(x + RIGHT_X[direction], y + RIGHT_Y[direction])) {
            next = direction;
        }
        if (next != direction) {
            const bool closing = x == start_x && y == start_y;
            if (!closing) {
                outline.push_back({static_cast<double>(x), static_cast<double>(y)});
            }
            direction = next;
        }
    } while (x != start_x || y != start_y || direction != 0);
    return outline;
}

int PixelValue(const cv::Mat& gray, int x, int y) {
    return (gray.depth() == CV_16U) ? gray.at<uint16_t>(y, x) : gray.at<uchar>(y, x);
}

} // namespace

ThresholdSelection SelectThresholdRegions(const cv::Mat& gray, int min_value, int max_value, int min_pixels, int max_regions) {
    CheckImage(gray);
    if (min_value > max_value) {
        throw std::invalid_argument("The threshold minimum must not be above its maximum");
    }
    if (min_pixels < 1) {
        throw std::invalid_argument("The minimum region size must be at least 1 pixel");
    }
    if (max_regions < 0) {
        throw std::invalid_argument("The maximum number of regions must not be negative");
    }

    const cv::Mat filled = FillHoles(RangeMask(gray, min_value, max_value));
    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    const int count = cv::connectedComponentsWithStats(filled, labels, stats, centroids, 8, CV_32S);

    std::vector<int> candidates;
    for (int label = 1; label < count; ++label) {
        if (stats.at<int>(label, cv::CC_STAT_AREA) >= min_pixels) {
            candidates.push_back(label);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [&stats](int a, int b) {
        const auto key = [&stats](int label) {
            return std::make_tuple(-stats.at<int>(label, cv::CC_STAT_AREA), stats.at<int>(label, cv::CC_STAT_TOP),
                stats.at<int>(label, cv::CC_STAT_LEFT), label);
        };
        return key(a) < key(b);
    });

    ThresholdSelection selection;
    selection.total = static_cast<int>(candidates.size());
    const size_t returned = std::min(candidates.size(), static_cast<size_t>(max_regions));
    for (size_t i = 0; i < returned; ++i) {
        const int label = candidates[i];
        SelectedRegion region;
        region.pixel_count = stats.at<int>(label, cv::CC_STAT_AREA);
        region.box = cv::Rect(stats.at<int>(label, cv::CC_STAT_LEFT), stats.at<int>(label, cv::CC_STAT_TOP),
            stats.at<int>(label, cv::CC_STAT_WIDTH), stats.at<int>(label, cv::CC_STAT_HEIGHT));
        // The first pixel in raster order lies in the top row of the bounding box
        const int* top_row = labels.ptr<int>(region.box.y);
        int start_x = region.box.x;
        while (top_row[start_x] != label) {
            ++start_x;
        }
        region.outline = TraceOutline(start_x, region.box.y, [&labels, label](int x, int y) {
            return x >= 0 && y >= 0 && x < labels.cols && y < labels.rows && labels.at<int>(y, x) == label;
        });
        selection.regions.push_back(std::move(region));
    }
    return selection;
}

std::optional<SelectedRegion> SelectWandRegion(const cv::Mat& gray, int x, int y, int tolerance) {
    CheckImage(gray);
    if (tolerance < 0) {
        throw std::invalid_argument("The tolerance must not be negative");
    }
    if (x < 0 || y < 0 || x >= gray.cols || y >= gray.rows) {
        return std::nullopt;
    }

    const int64_t seed = PixelValue(gray, x, y);
    cv::Mat mask = RangeMask(gray, seed - tolerance, seed + tolerance);
    cv::Rect box;
    cv::floodFill(mask, cv::Point(x, y), cv::Scalar(FLOODED), &box, cv::Scalar(0), cv::Scalar(0), 8);
    const cv::Mat region = FillHoles(mask(box) == FLOODED);

    SelectedRegion result;
    result.box = box;
    result.pixel_count = cv::countNonZero(region);
    const uchar* top_row = region.ptr<uchar>(0);
    int start_x = 0;
    while (top_row[start_x] != INSIDE) {
        ++start_x;
    }
    result.outline = TraceOutline(start_x + box.x, box.y, [&region, &box](int px, int py) {
        const int local_x = px - box.x;
        const int local_y = py - box.y;
        return local_x >= 0 && local_y >= 0 && local_x < region.cols && local_y < region.rows &&
               region.at<uchar>(local_y, local_x) == INSIDE;
    });
    return result;
}

} // namespace glcm
