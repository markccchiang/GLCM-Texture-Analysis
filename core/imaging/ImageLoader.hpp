#ifndef GLCM_IMAGE_LOADER_HPP_
#define GLCM_IMAGE_LOADER_HPP_

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace glcm {

struct ImageInfo {
    int width = 0;
    int height = 0;
    int bit_depth = 0;       // 8 or 16
    int source_channels = 0; // channels of the decoded file before grayscale conversion (1 or 3)
};

// A decoded image, ready for analysis: single-channel CV_8UC1 or CV_16UC1
struct LoadedImage {
    cv::Mat gray;
    ImageInfo info;
    std::vector<std::string> warnings; // e.g. "Color image converted to grayscale"
};

// Reads an image file (PNG, JPEG, BMP, 8/16-bit TIFF, ...). The bit depth is kept, color images are converted to
// grayscale, an alpha channel is ignored and the EXIF orientation is applied.
// Throws std::runtime_error if the file cannot be read or decoded, and std::invalid_argument for bit depths other than
// 8 and 16.
LoadedImage LoadImageFile(const std::string& path);

// Same as LoadImageFile, for an encoded image held in memory (e.g. an upload)
LoadedImage LoadImageBytes(const std::vector<uchar>& bytes);

} // namespace glcm

#endif // GLCM_IMAGE_LOADER_HPP_
