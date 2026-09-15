#include "imaging/PngEncoder.hpp"

#include <zlib.h>

#include <array>
#include <cmath>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>

namespace glcm {

namespace {

// Signature (8 bytes) and IHDR chunk (4 length + 4 type + 13 data + 4 CRC)
constexpr size_t AFTER_IHDR = 33;
constexpr double MM_PER_METRE = 1000.0;

void AppendUint32(std::vector<uchar>& out, uint32_t value) {
    out.push_back(static_cast<uchar>(value >> 24));
    out.push_back(static_cast<uchar>(value >> 16));
    out.push_back(static_cast<uchar>(value >> 8));
    out.push_back(static_cast<uchar>(value));
}

} // namespace

std::vector<uchar> EncodePng(const cv::Mat& gray, const std::optional<PixelSpacing>& spacing) {
    std::vector<uchar> png;
    if (!cv::imencode(".png", gray, png) || png.size() < AFTER_IHDR) {
        throw std::runtime_error("Cannot encode the image as PNG");
    }
    if (!spacing) {
        return png;
    }
    const double x_per_metre = std::round(MM_PER_METRE / spacing->x_mm);
    const double y_per_metre = std::round(MM_PER_METRE / spacing->y_mm);
    if (!(x_per_metre >= 1 && y_per_metre >= 1 && x_per_metre <= 0xFFFFFFFF && y_per_metre <= 0xFFFFFFFF)) {
        return png;
    }

    std::vector<uchar> chunk;
    AppendUint32(chunk, 9);
    for (char c : {'p', 'H', 'Y', 's'}) {
        chunk.push_back(static_cast<uchar>(c));
    }
    AppendUint32(chunk, static_cast<uint32_t>(x_per_metre));
    AppendUint32(chunk, static_cast<uint32_t>(y_per_metre));
    chunk.push_back(1); // unit: metre
    // The CRC covers the chunk type and data
    const uLong crc = crc32(crc32(0L, Z_NULL, 0), chunk.data() + 4, static_cast<uInt>(chunk.size() - 4));
    AppendUint32(chunk, static_cast<uint32_t>(crc));

    png.insert(png.begin() + AFTER_IHDR, chunk.begin(), chunk.end());
    return png;
}

} // namespace glcm
