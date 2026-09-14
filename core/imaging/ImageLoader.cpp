#include "imaging/ImageLoader.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <optional>
#include <stdexcept>

#include "imaging/ImageHeader.hpp"

namespace glcm {

namespace {

// Keep the bit depth and color channels, apply the EXIF orientation, drop alpha
const int DECODE_FLAGS = cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR;

// Checks the size read from the header before decoding
void CheckHeaderSize(const std::optional<ImageSize>& size, int64_t max_pixels) {
    if (max_pixels <= 0) {
        return;
    }
    if (!size) {
        // Not recognized as an image whose size can be checked; reported like a file that cannot be decoded
        throw std::runtime_error("Unknown image format: only PNG, JPEG, BMP and TIFF images can be checked against the pixel limit");
    }
    // width * height > max_pixels, without overflowing for absurd sizes
    if (size->width > max_pixels / size->height) {
        throw ImageTooLargeError(size->width, size->height, max_pixels);
    }
}

LoadedImage ToLoadedImage(const cv::Mat& decoded, int64_t max_pixels) {
    // The decoder may disagree with the header, e.g. for a multi-image TIFF
    if (max_pixels > 0 && static_cast<int64_t>(decoded.total()) > max_pixels) {
        throw ImageTooLargeError(decoded.cols, decoded.rows, max_pixels);
    }
    if (decoded.depth() != CV_8U && decoded.depth() != CV_16U) {
        throw std::invalid_argument("Unsupported image bit depth: only 8-bit and 16-bit images are supported");
    }

    LoadedImage result;
    result.info.source_channels = decoded.channels();
    switch (decoded.channels()) {
        case 1:
            result.gray = decoded;
            break;
        case 3:
            cv::cvtColor(decoded, result.gray, cv::COLOR_BGR2GRAY);
            result.warnings.push_back("Color image converted to grayscale");
            break;
        case 4:
            cv::cvtColor(decoded, result.gray, cv::COLOR_BGRA2GRAY);
            result.warnings.push_back("Color image converted to grayscale; alpha channel ignored");
            break;
        default:
            throw std::invalid_argument("Unsupported number of image channels: " + std::to_string(decoded.channels()));
    }

    result.info.width = result.gray.cols;
    result.info.height = result.gray.rows;
    result.info.bit_depth = (result.gray.depth() == CV_16U) ? 16 : 8;
    return result;
}

} // namespace

ImageTooLargeError::ImageTooLargeError(int64_t width, int64_t height, int64_t max_pixels)
    : std::runtime_error("The image has " + std::to_string(width) + " x " + std::to_string(height) + " pixels, more than the limit of " +
                         std::to_string(max_pixels) + " pixels") {}

LoadedImage LoadImageFile(const std::string& path, int64_t max_pixels) {
    if (max_pixels > 0) {
        CheckHeaderSize(ReadImageSize(path), max_pixels);
    }
    cv::Mat decoded = cv::imread(path, DECODE_FLAGS);
    if (decoded.empty()) {
        throw std::runtime_error("Cannot read the image: " + path);
    }
    return ToLoadedImage(decoded, max_pixels);
}

LoadedImage LoadImageBytes(const std::vector<uchar>& bytes, int64_t max_pixels) {
    if (bytes.empty()) {
        throw std::runtime_error("Cannot decode an empty image buffer");
    }
    if (max_pixels > 0) {
        CheckHeaderSize(ReadImageSizeFromBytes(bytes), max_pixels);
    }
    cv::Mat decoded = cv::imdecode(bytes, DECODE_FLAGS);
    if (decoded.empty()) {
        throw std::runtime_error("Cannot decode the image data");
    }
    return ToLoadedImage(decoded, max_pixels);
}

} // namespace glcm
