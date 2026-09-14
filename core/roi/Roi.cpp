#include "roi/Roi.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace glcm {

namespace {

const uchar INSIDE = 255;

void RequireFinite(std::initializer_list<double> values, const char* shape) {
    for (double value : values) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(std::string(shape) + " ROI has a non-finite coordinate");
        }
    }
}

void RequireImageSize(cv::Size image_size) {
    if (image_size.width <= 0 || image_size.height <= 0) {
        throw std::invalid_argument("The image size must be positive");
    }
}

// Clamps a (possibly huge) floating-point index to [0, size] before converting it to int. The callers never pass NaN;
// if one did, it maps to 0 instead of an undefined conversion.
int ClampIndex(double index, int size) {
    if (std::isnan(index)) {
        return 0;
    }
    return static_cast<int>(std::clamp(index, 0.0, static_cast<double>(size)));
}

// Index of the first pixel whose centre (index + 0.5) is at or after the edge, clamped to [0, size]. The pixels whose
// centres lie in the half-open interval [a, b) are [FirstCentreAtOrAfter(a), FirstCentreAtOrAfter(b)).
int FirstCentreAtOrAfter(double edge, int size) {
    return ClampIndex(std::ceil(edge - 0.5), size);
}

// Half-open pixel ranges in image coordinates, clamped to the image, containing every pixel a shape can cover
struct PixelRange {
    int first_col = 0;
    int end_col = 0;
    int first_row = 0;
    int end_row = 0;
};

cv::Rect ToRect(const PixelRange& range) {
    if (range.first_col >= range.end_col || range.first_row >= range.end_row) {
        return cv::Rect();
    }
    return cv::Rect(range.first_col, range.first_row, range.end_col - range.first_col, range.end_row - range.first_row);
}

// The fill functions compute pixel ranges in image coordinates, exactly as for a full-size mask, and write into `mask`,
// which covers the image pixels [origin.x, origin.x + mask.cols) x [origin.y, origin.y + mask.rows). Writes outside that
// area are skipped.

PixelRange Range(const RectangleRoi& rectangle, cv::Size image_size) {
    RequireFinite({rectangle.x, rectangle.y, rectangle.width, rectangle.height}, "Rectangle");

    const double left = std::min(rectangle.x, rectangle.x + rectangle.width);
    const double right = std::max(rectangle.x, rectangle.x + rectangle.width);
    const double top = std::min(rectangle.y, rectangle.y + rectangle.height);
    const double bottom = std::max(rectangle.y, rectangle.y + rectangle.height);
    return {FirstCentreAtOrAfter(left, image_size.width), FirstCentreAtOrAfter(right, image_size.width),
        FirstCentreAtOrAfter(top, image_size.height), FirstCentreAtOrAfter(bottom, image_size.height)};
}

void Fill(const RectangleRoi& rectangle, cv::Size image_size, cv::Mat& mask, cv::Point origin) {
    const cv::Rect area = ToRect(Range(rectangle, image_size)) & cv::Rect(origin, mask.size());
    if (area.area() > 0) {
        mask(area - origin).setTo(cv::Scalar(INSIDE));
    }
}

struct EllipseGeometry {
    bool empty = true;
    double cos_theta = 1.0;
    double sin_theta = 0.0;
    double rx2 = 0.0;
    double ry2 = 0.0;
    PixelRange range;
};

EllipseGeometry Geometry(const EllipseRoi& ellipse, cv::Size image_size) {
    RequireFinite({ellipse.cx, ellipse.cy, ellipse.rx, ellipse.ry, ellipse.angle_deg}, "Ellipse");
    EllipseGeometry geometry;
    if (ellipse.rx <= 0.0 || ellipse.ry <= 0.0) {
        return geometry;
    }

    const double theta = ellipse.angle_deg * CV_PI / 180.0;
    geometry.empty = false;
    geometry.cos_theta = std::cos(theta);
    geometry.sin_theta = std::sin(theta);
    geometry.rx2 = ellipse.rx * ellipse.rx;
    geometry.ry2 = ellipse.ry * ellipse.ry;

    // Half extents of the rotated ellipse's bounding box, used only to limit the pixels tested. std::hypot avoids squaring
    // the radii: for a huge radius the square overflows, and sqrt(inf * 0) would be NaN when the angle is 0 or 90 degrees.
    const double half_width = std::hypot(ellipse.rx * geometry.cos_theta, ellipse.ry * geometry.sin_theta);
    const double half_height = std::hypot(ellipse.rx * geometry.sin_theta, ellipse.ry * geometry.cos_theta);
    geometry.range = {ClampIndex(std::floor(ellipse.cx - half_width), image_size.width),
        ClampIndex(std::ceil(ellipse.cx + half_width) + 1.0, image_size.width),
        ClampIndex(std::floor(ellipse.cy - half_height), image_size.height),
        ClampIndex(std::ceil(ellipse.cy + half_height) + 1.0, image_size.height)};
    return geometry;
}

PixelRange Range(const EllipseRoi& ellipse, cv::Size image_size) {
    return Geometry(ellipse, image_size).range;
}

void Fill(const EllipseRoi& ellipse, cv::Size image_size, cv::Mat& mask, cv::Point origin) {
    const EllipseGeometry geometry = Geometry(ellipse, image_size);
    if (geometry.empty) {
        return;
    }
    const cv::Rect area = ToRect(geometry.range) & cv::Rect(origin, mask.size());
    for (int row = area.y; row < area.y + area.height; ++row) {
        const double dy = (row + 0.5) - ellipse.cy;
        uchar* line = mask.ptr<uchar>(row - origin.y);
        for (int col = area.x; col < area.x + area.width; ++col) {
            const double dx = (col + 0.5) - ellipse.cx;
            // Coordinates in the ellipse's own (unrotated) axes
            const double u = dx * geometry.cos_theta + dy * geometry.sin_theta;
            const double v = -dx * geometry.sin_theta + dy * geometry.cos_theta;
            if (u * u / geometry.rx2 + v * v / geometry.ry2 <= 1.0) {
                line[col - origin.x] = INSIDE;
            }
        }
    }
}

PixelRange Range(const PolygonRoi& polygon, cv::Size image_size) {
    for (const auto& point : polygon.points) {
        RequireFinite({point[0], point[1]}, "Polygon");
    }
    if (polygon.points.size() < 3) {
        return {};
    }

    double min_x = polygon.points[0][0];
    double max_x = polygon.points[0][0];
    double min_y = polygon.points[0][1];
    double max_y = polygon.points[0][1];
    for (const auto& point : polygon.points) {
        min_x = std::min(min_x, point[0]);
        max_x = std::max(max_x, point[0]);
        min_y = std::min(min_y, point[1]);
        max_y = std::max(max_y, point[1]);
    }
    // Rows as scanned by Fill. A crossing lies between the x coordinates of its edge's end points (up to rounding), so
    // the vertex extents with a margin of one pixel on each side contain every filled column.
    return {ClampIndex(std::floor(min_x) - 1.0, image_size.width), ClampIndex(std::ceil(max_x) + 2.0, image_size.width),
        ClampIndex(std::floor(min_y), image_size.height), ClampIndex(std::ceil(max_y) + 1.0, image_size.height)};
}

void Fill(const PolygonRoi& polygon, cv::Size image_size, cv::Mat& mask, cv::Point origin) {
    const PixelRange range = Range(polygon, image_size);
    const size_t count = polygon.points.size();
    if (count < 3) {
        return;
    }
    const int first_row = std::max(range.first_row, origin.y);
    const int end_row = std::min(range.end_row, origin.y + mask.rows);
    const int first_target_col = origin.x;
    const int end_target_col = origin.x + mask.cols;

    // Scanline fill at every pixel-centre row. An edge crosses the scanline y if y is in [min(ya, yb), max(ya, yb)),
    // so shared vertices are counted once and horizontal edges never cross.
    std::vector<double> crossings;
    for (int row = first_row; row < end_row; ++row) {
        const double y = row + 0.5;
        crossings.clear();
        for (size_t i = 0; i < count; ++i) {
            const auto& a = polygon.points[i];
            const auto& b = polygon.points[(i + 1) % count];
            if ((a[1] <= y && y < b[1]) || (b[1] <= y && y < a[1])) {
                double x = a[0] + (y - a[1]) * (b[0] - a[0]) / (b[1] - a[1]);
                if (!std::isfinite(x)) {
                    // Vertices far outside the image, where b[0] - a[0] overflows (0 * inf is NaN). Interpolate without
                    // that difference: t is in [0, 1], so the crossing is finite or +-inf, which ClampIndex handles.
                    const double t = (y - a[1]) / (b[1] - a[1]);
                    x = a[0] * (1.0 - t) + b[0] * t;
                }
                crossings.push_back(x);
            }
        }
        std::sort(crossings.begin(), crossings.end());

        uchar* line = mask.ptr<uchar>(row - origin.y);
        for (size_t k = 0; k + 1 < crossings.size(); k += 2) {
            const int first_col = std::max(FirstCentreAtOrAfter(crossings[k], image_size.width), first_target_col);
            const int end_col = std::min(FirstCentreAtOrAfter(crossings[k + 1], image_size.width), end_target_col);
            if (first_col < end_col) {
                std::fill(line + (first_col - origin.x), line + (end_col - origin.x), INSIDE);
            }
        }
    }
}

void RequireMask(const cv::Mat& mask) {
    if (mask.type() != CV_8UC1) {
        throw std::invalid_argument("The mask must be an 8-bit single-channel image (CV_8UC1)");
    }
}

} // namespace

cv::Mat RasterizeMask(const RoiShape& shape, cv::Size image_size) {
    RequireImageSize(image_size);
    cv::Mat mask = cv::Mat::zeros(image_size, CV_8UC1);
    std::visit([&](const auto& s) { Fill(s, image_size, mask, cv::Point(0, 0)); }, shape);
    return mask;
}

CroppedMask RasterizeCroppedMask(const RoiShape& shape, cv::Size image_size) {
    RequireImageSize(image_size);
    CroppedMask cropped;
    cropped.box = std::visit([&](const auto& s) { return ToRect(Range(s, image_size)); }, shape);
    if (cropped.box.area() == 0) {
        cropped.box = cv::Rect();
        return cropped;
    }
    cropped.mask = cv::Mat::zeros(cropped.box.size(), CV_8UC1);
    std::visit([&](const auto& s) { Fill(s, image_size, cropped.mask, cropped.box.tl()); }, shape);
    return cropped;
}

cv::Rect MaskBoundingBox(const cv::Mat& mask) {
    RequireMask(mask);
    int min_col = mask.cols;
    int min_row = mask.rows;
    int max_col = -1;
    int max_row = -1;
    for (int row = 0; row < mask.rows; ++row) {
        const uchar* line = mask.ptr<uchar>(row);
        for (int col = 0; col < mask.cols; ++col) {
            if (line[col] != 0) {
                min_col = std::min(min_col, col);
                max_col = std::max(max_col, col);
                min_row = std::min(min_row, row);
                max_row = std::max(max_row, row);
            }
        }
    }
    if (max_col < 0) {
        return cv::Rect();
    }
    return cv::Rect(min_col, min_row, max_col - min_col + 1, max_row - min_row + 1);
}

int CountMaskPixels(const cv::Mat& mask) {
    RequireMask(mask);
    return mask.empty() ? 0 : cv::countNonZero(mask);
}

} // namespace glcm
