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

void Fill(const RectangleRoi& rectangle, cv::Mat& mask) {
    RequireFinite({rectangle.x, rectangle.y, rectangle.width, rectangle.height}, "Rectangle");

    const double left = std::min(rectangle.x, rectangle.x + rectangle.width);
    const double right = std::max(rectangle.x, rectangle.x + rectangle.width);
    const double top = std::min(rectangle.y, rectangle.y + rectangle.height);
    const double bottom = std::max(rectangle.y, rectangle.y + rectangle.height);

    const int first_col = FirstCentreAtOrAfter(left, mask.cols);
    const int end_col = FirstCentreAtOrAfter(right, mask.cols);
    const int first_row = FirstCentreAtOrAfter(top, mask.rows);
    const int end_row = FirstCentreAtOrAfter(bottom, mask.rows);

    if (first_col < end_col && first_row < end_row) {
        mask(cv::Range(first_row, end_row), cv::Range(first_col, end_col)).setTo(cv::Scalar(INSIDE));
    }
}

void Fill(const EllipseRoi& ellipse, cv::Mat& mask) {
    RequireFinite({ellipse.cx, ellipse.cy, ellipse.rx, ellipse.ry, ellipse.angle_deg}, "Ellipse");
    if (ellipse.rx <= 0.0 || ellipse.ry <= 0.0) {
        return;
    }

    const double theta = ellipse.angle_deg * CV_PI / 180.0;
    const double cos_theta = std::cos(theta);
    const double sin_theta = std::sin(theta);
    const double rx2 = ellipse.rx * ellipse.rx;
    const double ry2 = ellipse.ry * ellipse.ry;

    // Half extents of the rotated ellipse's bounding box, used only to limit the pixels tested. std::hypot avoids squaring
    // the radii: for a huge radius the square overflows, and sqrt(inf * 0) would be NaN when the angle is 0 or 90 degrees.
    const double half_width = std::hypot(ellipse.rx * cos_theta, ellipse.ry * sin_theta);
    const double half_height = std::hypot(ellipse.rx * sin_theta, ellipse.ry * cos_theta);
    const int first_col = ClampIndex(std::floor(ellipse.cx - half_width), mask.cols);
    const int end_col = ClampIndex(std::ceil(ellipse.cx + half_width) + 1.0, mask.cols);
    const int first_row = ClampIndex(std::floor(ellipse.cy - half_height), mask.rows);
    const int end_row = ClampIndex(std::ceil(ellipse.cy + half_height) + 1.0, mask.rows);

    for (int row = first_row; row < end_row; ++row) {
        const double dy = (row + 0.5) - ellipse.cy;
        uchar* line = mask.ptr<uchar>(row);
        for (int col = first_col; col < end_col; ++col) {
            const double dx = (col + 0.5) - ellipse.cx;
            // Coordinates in the ellipse's own (unrotated) axes
            const double u = dx * cos_theta + dy * sin_theta;
            const double v = -dx * sin_theta + dy * cos_theta;
            if (u * u / rx2 + v * v / ry2 <= 1.0) {
                line[col] = INSIDE;
            }
        }
    }
}

void Fill(const PolygonRoi& polygon, cv::Mat& mask) {
    for (const auto& point : polygon.points) {
        RequireFinite({point[0], point[1]}, "Polygon");
    }
    const size_t count = polygon.points.size();
    if (count < 3) {
        return;
    }

    double min_y = polygon.points[0][1];
    double max_y = polygon.points[0][1];
    for (const auto& point : polygon.points) {
        min_y = std::min(min_y, point[1]);
        max_y = std::max(max_y, point[1]);
    }
    const int first_row = ClampIndex(std::floor(min_y), mask.rows);
    const int end_row = ClampIndex(std::ceil(max_y) + 1.0, mask.rows);

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

        uchar* line = mask.ptr<uchar>(row);
        for (size_t k = 0; k + 1 < crossings.size(); k += 2) {
            const int first_col = FirstCentreAtOrAfter(crossings[k], mask.cols);
            const int end_col = FirstCentreAtOrAfter(crossings[k + 1], mask.cols);
            if (first_col < end_col) {
                std::fill(line + first_col, line + end_col, INSIDE);
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
    if (image_size.width <= 0 || image_size.height <= 0) {
        throw std::invalid_argument("The image size must be positive");
    }
    cv::Mat mask = cv::Mat::zeros(image_size, CV_8UC1);
    std::visit([&mask](const auto& s) { Fill(s, mask); }, shape);
    return mask;
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
    return cv::countNonZero(mask);
}

} // namespace glcm
