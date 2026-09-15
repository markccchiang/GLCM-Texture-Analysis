#include "roi/Livewire.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>

#include "imaging/EdgeDetection.hpp"

namespace glcm {

namespace {

// Keeps a path along a flat area slightly cheaper to shorten than to detour
const double BASE_COST = 0.05;
const double DIAGONAL = std::sqrt(2.0);

} // namespace

std::vector<std::array<double, 2>> LivewirePath(const cv::Mat& gray, cv::Point from, cv::Point to, double sigma) {
    if (gray.empty() || gray.channels() != 1 || (gray.depth() != CV_8U && gray.depth() != CV_16U)) {
        throw std::invalid_argument("The image must be an 8- or 16-bit single-channel image");
    }
    const cv::Rect image(0, 0, gray.cols, gray.rows);
    if (!image.contains(from) || !image.contains(to)) {
        throw std::invalid_argument("The livewire points must lie inside the image");
    }
    if (std::abs(from.x - to.x) > MAX_LIVEWIRE_SPAN || std::abs(from.y - to.y) > MAX_LIVEWIRE_SPAN) {
        throw std::invalid_argument(
            "The livewire points must be at most " + std::to_string(MAX_LIVEWIRE_SPAN) + " pixels apart along each axis");
    }
    if (!std::isfinite(sigma) || sigma < 0.0 || sigma > MAX_EDGE_SIGMA) {
        throw std::invalid_argument("The smoothing sigma must be between 0 and 10 pixels");
    }

    // The gradient is computed with a border around the box, so pixels at its edge see their real neighbours
    const cv::Rect box = cv::Rect(cv::Point(std::min(from.x, to.x) - LIVEWIRE_MARGIN, std::min(from.y, to.y) - LIVEWIRE_MARGIN),
                             cv::Point(std::max(from.x, to.x) + LIVEWIRE_MARGIN + 1, std::max(from.y, to.y) + LIVEWIRE_MARGIN + 1)) &
                         image;
    const int border = static_cast<int>(std::ceil(3.0 * sigma)) + 2;
    const cv::Rect padded = cv::Rect(box.x - border, box.y - border, box.width + 2 * border, box.height + 2 * border) & image;
    const cv::Mat magnitude = GradientMagnitude(gray(padded), sigma)(box - padded.tl());
    double largest = 0.0;
    cv::minMaxLoc(magnitude, nullptr, &largest);

    const int width = box.width;
    const int count = box.width * box.height;
    std::vector<double> cost(static_cast<size_t>(count));
    for (int y = 0; y < box.height; ++y) {
        const float* row = magnitude.ptr<float>(y);
        for (int x = 0; x < width; ++x) {
            const double relative = largest > 0.0 ? row[x] / largest : 0.0;
            cost[static_cast<size_t>(y * width + x)] = 1.0 + BASE_COST - relative;
        }
    }

    const int start = (from.y - box.y) * width + (from.x - box.x);
    const int goal = (to.y - box.y) * width + (to.x - box.x);
    std::vector<double> distance(static_cast<size_t>(count), std::numeric_limits<double>::infinity());
    std::vector<int> previous(static_cast<size_t>(count), -1);
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
    distance[static_cast<size_t>(start)] = 0.0;
    queue.emplace(0.0, start);
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static const double LENGTH[8] = {1.0, 1.0, 1.0, 1.0, DIAGONAL, DIAGONAL, DIAGONAL, DIAGONAL};
    while (!queue.empty()) {
        const auto [current_distance, current] = queue.top();
        queue.pop();
        if (current_distance > distance[static_cast<size_t>(current)]) {
            continue;
        }
        if (current == goal) {
            break;
        }
        const int x = current % width;
        const int y = current / width;
        for (int k = 0; k < 8; ++k) {
            const int nx = x + DX[k];
            const int ny = y + DY[k];
            if (nx < 0 || ny < 0 || nx >= width || ny >= box.height) {
                continue;
            }
            const int next = ny * width + nx;
            const double candidate = current_distance + LENGTH[k] * cost[static_cast<size_t>(next)];
            if (candidate < distance[static_cast<size_t>(next)]) {
                distance[static_cast<size_t>(next)] = candidate;
                previous[static_cast<size_t>(next)] = current;
                queue.emplace(candidate, next);
            }
        }
    }

    std::vector<cv::Point> pixels;
    for (int node = goal; node != -1; node = previous[static_cast<size_t>(node)]) {
        pixels.emplace_back(node % width + box.x, node / width + box.y);
        if (node == start) {
            break;
        }
    }
    std::reverse(pixels.begin(), pixels.end());

    std::vector<std::array<double, 2>> path;
    for (size_t i = 0; i < pixels.size(); ++i) {
        const bool end = i == 0 || i + 1 == pixels.size();
        const bool turns = !end && (pixels[i] - pixels[i - 1]) != (pixels[i + 1] - pixels[i]);
        if (end || turns) {
            path.push_back({pixels[i].x + 0.5, pixels[i].y + 0.5});
        }
    }
    return path;
}

} // namespace glcm
