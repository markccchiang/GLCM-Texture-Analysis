#ifndef GLCM_IMAGE_LOADER_HPP_
#define GLCM_IMAGE_LOADER_HPP_

#include "imaging/ImageHeader.hpp"

#include <cstdint>
#include <opencv2/core.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace glcm {

struct ImageInfo {
    int width = 0;
    int height = 0;
    int bit_depth = 0;       // 8 or 16
    int source_channels = 0; // channels of the decoded file before grayscale conversion (1 or 3)
    // Millimetres per pixel from the file's resolution (see ImageSize::pixel_spacing), after the EXIF orientation
    std::optional<PixelSpacing> pixel_spacing;
};

// A decoded image, ready for analysis: single-channel CV_8UC1 or CV_16UC1
struct LoadedImage {
    cv::Mat gray;
    ImageInfo info;
    std::vector<std::string> warnings; // e.g. "Color image converted to grayscale"
};

// Thrown when an image has more pixels than allowed. When the limit is checked from the header (see LoadImageFile), the
// pixels have not been decoded.
class ImageTooLargeError : public std::runtime_error {
public:
    ImageTooLargeError(int64_t width, int64_t height, int64_t max_pixels);
};

// Reads an image file (PNG, JPEG, BMP, 8/16-bit TIFF, ...). The bit depth is kept, color images are converted to
// grayscale, an alpha channel is ignored and the EXIF orientation is applied.
// Throws std::runtime_error if the file cannot be read or decoded, and std::invalid_argument for bit depths other than
// 8 and 16.
// With max_pixels > 0, the size is first read from the header (imaging/ImageHeader.hpp), so an image above the limit
// throws ImageTooLargeError before the decoder allocates memory for it; files that are not PNG, JPEG, BMP or TIFF then
// throw std::runtime_error, because their size cannot be checked in advance.
LoadedImage LoadImageFile(const std::string& path, int64_t max_pixels = 0);

// Same as LoadImageFile, for an encoded image held in memory (e.g. an upload)
LoadedImage LoadImageBytes(const std::vector<uchar>& bytes, int64_t max_pixels = 0);

} // namespace glcm

#endif // GLCM_IMAGE_LOADER_HPP_
