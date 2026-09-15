#include "imaging/EdgeDetection.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <vector>

namespace glcm {

namespace {

// The 3x3 Sobel derivative of a ramp rising by 1 per pixel is 8
const double SOBEL_SCALE = 8.0;
const size_t MAX_STATISTICS_SAMPLES = 1000000;

void CheckImage(const cv::Mat& gray) {
    if (gray.empty() || gray.channels() != 1 || (gray.depth() != CV_8U && gray.depth() != CV_16U)) {
        throw std::invalid_argument("The image must be an 8- or 16-bit single-channel image");
    }
}

void CheckSigma(double sigma) {
    if (!std::isfinite(sigma) || sigma < 0.0 || sigma > MAX_EDGE_SIGMA) {
        throw std::invalid_argument("The smoothing sigma must be between 0 and 10 pixels");
    }
}

cv::Mat Smoothed(const cv::Mat& gray, double sigma) {
    cv::Mat image;
    gray.convertTo(image, CV_32F);
    if (sigma > 0.0) {
        cv::GaussianBlur(image, image, cv::Size(), sigma, sigma, cv::BORDER_REFLECT_101);
    }
    return image;
}

void Derivatives(const cv::Mat& gray, double sigma, cv::Mat& gx, cv::Mat& gy) {
    const cv::Mat image = Smoothed(gray, sigma);
    cv::Sobel(image, gx, CV_32F, 1, 0, 3, 1.0, 0.0, cv::BORDER_REFLECT_101);
    cv::Sobel(image, gy, CV_32F, 0, 1, 3, 1.0, 0.0, cv::BORDER_REFLECT_101);
}

cv::Mat Reduced(const cv::Mat& map, int max_size, bool any_edge) {
    const int long_side = std::max(map.cols, map.rows);
    if (max_size <= 0 || long_side <= max_size) {
        return map;
    }
    const double factor = static_cast<double>(max_size) / long_side;
    const cv::Size size(
        std::max(1, static_cast<int>(std::lround(map.cols * factor))), std::max(1, static_cast<int>(std::lround(map.rows * factor))));
    cv::Mat reduced;
    if (any_edge) {
        // Area averaging is above zero wherever the covered pixels include an edge
        cv::Mat values;
        map.convertTo(values, CV_32F);
        cv::resize(values, values, size, 0.0, 0.0, cv::INTER_AREA);
        reduced = values > 0.0F;
    } else {
        cv::resize(map, reduced, size, 0.0, 0.0, cv::INTER_AREA);
    }
    return reduced;
}

} // namespace

cv::Mat GradientMagnitude(const cv::Mat& gray, double sigma) {
    CheckImage(gray);
    CheckSigma(sigma);
    cv::Mat gx;
    cv::Mat gy;
    Derivatives(gray, sigma, gx, gy);
    cv::Mat magnitude;
    cv::magnitude(gx, gy, magnitude);
    return magnitude / SOBEL_SCALE;
}

GradientStatistics ComputeGradientStatistics(const cv::Mat& gray, double sigma) {
    const cv::Mat magnitude = GradientMagnitude(gray, sigma);
    const size_t pixels = static_cast<size_t>(magnitude.rows) * static_cast<size_t>(magnitude.cols);
    const int stride = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(pixels) / MAX_STATISTICS_SAMPLES))));
    std::vector<float> samples;
    samples.reserve(pixels / static_cast<size_t>(stride * stride) + static_cast<size_t>(magnitude.rows + magnitude.cols));
    for (int y = 0; y < magnitude.rows; y += stride) {
        const float* row = magnitude.ptr<float>(y);
        for (int x = 0; x < magnitude.cols; x += stride) {
            samples.push_back(row[x]);
        }
    }
    const auto percentile = [&samples](double fraction) {
        const size_t rank =
            std::clamp<size_t>(static_cast<size_t>(std::ceil(fraction * static_cast<double>(samples.size()))), 1, samples.size());
        std::nth_element(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(rank - 1), samples.end());
        return static_cast<double>(samples[rank - 1]);
    };
    GradientStatistics statistics;
    statistics.p50 = percentile(0.50);
    statistics.p90 = percentile(0.90);
    statistics.p95 = percentile(0.95);
    statistics.p99 = percentile(0.99);
    cv::minMaxLoc(magnitude, nullptr, &statistics.max);
    return statistics;
}

cv::Mat RenderEdgeMap(const cv::Mat& gray, EdgeMethod method, double sigma, double low, double high, int max_size) {
    CheckImage(gray);
    CheckSigma(sigma);
    if (!std::isfinite(low) || !std::isfinite(high)) {
        throw std::invalid_argument("The edge map limits must be finite numbers");
    }
    if (max_size < 0) {
        throw std::invalid_argument("The largest size must not be negative");
    }

    if (method == EdgeMethod::Sobel) {
        if (!(low < high)) {
            throw std::invalid_argument("The low value of the gradient window must be below the high value");
        }
        const cv::Mat magnitude = GradientMagnitude(gray, sigma);
        cv::Mat map;
        // convertTo rounds and saturates to [0, 255]
        magnitude.convertTo(map, CV_8U, 255.0 / (high - low), -low * 255.0 / (high - low));
        return Reduced(map, max_size, false);
    }

    if (low < 0.0 || low > high) {
        throw std::invalid_argument("The Canny thresholds must satisfy 0 <= low <= high");
    }
    cv::Mat gx;
    cv::Mat gy;
    Derivatives(gray, sigma, gx, gy);
    double largest_x = 0.0;
    double largest_y = 0.0;
    cv::minMaxLoc(cv::abs(gx), nullptr, &largest_x);
    cv::minMaxLoc(cv::abs(gy), nullptr, &largest_y);
    const double scale = std::min(1.0, 32767.0 / std::max({largest_x, largest_y, 1.0}));
    cv::Mat dx;
    cv::Mat dy;
    gx.convertTo(dx, CV_16S, scale);
    gy.convertTo(dy, CV_16S, scale);
    cv::Mat edges;
    cv::Canny(dx, dy, edges, low * SOBEL_SCALE * scale, high * SOBEL_SCALE * scale, true);
    return Reduced(edges, max_size, true);
}

} // namespace glcm
