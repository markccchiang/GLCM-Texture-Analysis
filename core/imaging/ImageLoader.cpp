#include "imaging/ImageLoader.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace glcm {

namespace {

// Keep the bit depth and color channels, apply the EXIF orientation, drop alpha
const int DECODE_FLAGS = cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR;

LoadedImage ToLoadedImage(const cv::Mat& decoded) {
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

LoadedImage LoadImageFile(const std::string& path) {
    cv::Mat decoded = cv::imread(path, DECODE_FLAGS);
    if (decoded.empty()) {
        throw std::runtime_error("Cannot read the image: " + path);
    }
    return ToLoadedImage(decoded);
}

LoadedImage LoadImageBytes(const std::vector<uchar>& bytes) {
    if (bytes.empty()) {
        throw std::runtime_error("Cannot decode an empty image buffer");
    }
    cv::Mat decoded = cv::imdecode(bytes, DECODE_FLAGS);
    if (decoded.empty()) {
        throw std::runtime_error("Cannot decode the image data");
    }
    return ToLoadedImage(decoded);
}

} // namespace glcm
