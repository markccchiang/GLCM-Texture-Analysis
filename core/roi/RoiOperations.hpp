#ifndef GLCM_ROI_OPERATIONS_HPP_
#define GLCM_ROI_OPERATIONS_HPP_

#include <array>
#include <opencv2/core.hpp>
#include <optional>
#include <vector>

#include "roi/Roi.hpp"

namespace glcm {

enum class RoiOperation {
    Union,   // the pixels of any shape
    Subtract // the pixels of the first shape that no other shape covers
};

// A shape computed on the pixel grid, as one polygon ROI
struct OperationResult {
    PolygonRoi polygon; // no points when no pixel is left
    int pixel_count = 0;
    cv::Rect box; // bounding box of the pixels; empty when none is left
};

// A polygon whose pixels (RasterizeMask, pixel-centre rule) are exactly the non-zero pixels of `mask`, with mask pixel
// (c, r) at image pixel (offset.x + c, offset.y + r). It consists of the outline of every 8-connected part and of every hole
// (background not 4-connected to the outside) along the pixel edges, joined into one polygon by straight cuts from loop to
// loop. Each cut is traversed once in each direction, so under the even-odd rule it adds no crossings and the polygon covers
// the parts without the holes. Empty for a mask without non-zero pixels. Throws std::invalid_argument for a mask that is not
// CV_8UC1.
std::vector<std::array<double, 2>> MaskOutline(const cv::Mat& mask, cv::Point offset = cv::Point());

// Union or subtraction of shapes rasterized on an image of the given size (shapes are clipped to the image). Throws
// std::invalid_argument for an empty list or an invalid shape.
OperationResult CombineShapes(const std::vector<RoiShape>& shapes, RoiOperation operation, cv::Size image_size);

// A brush stroke: the pixels whose centres lie within `radius` of the polyline `path` (a single point paints a disc),
// added to `shape`, or removed from it when `erase` is set. Without a shape, painting gives the stroke alone and erasing
// gives nothing. Throws std::invalid_argument for an empty path, a radius that is not positive and finite, non-finite path
// coordinates or an invalid shape.
OperationResult PaintStroke(
    const std::optional<RoiShape>& shape, const std::vector<std::array<double, 2>>& path, double radius, bool erase, cv::Size image_size);

} // namespace glcm

#endif // GLCM_ROI_OPERATIONS_HPP_
