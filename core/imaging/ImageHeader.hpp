#ifndef GLCM_IMAGE_HEADER_HPP_
#define GLCM_IMAGE_HEADER_HPP_

#include <cstdint>
#include <opencv2/core.hpp>
#include <optional>
#include <string>
#include <vector>

namespace glcm {

// Width and height declared by an image header. 64-bit, so absurd sizes in crafted files are reported as they are.
struct ImageSize {
    int64_t width = 0;
    int64_t height = 0;
};

// Reads the size from the header of a PNG, JPEG, BMP or TIFF (including BigTIFF) file without decoding the pixels, so
// the size can be checked before a decoder allocates memory for it.
// Returns nullopt for other formats. Throws std::runtime_error if the file cannot be read, or if the header of a
// recognized format is truncated or invalid.
std::optional<ImageSize> ReadImageSize(const std::string& path);

// Same as ReadImageSize, for an encoded image held in memory
std::optional<ImageSize> ReadImageSizeFromBytes(const std::vector<uchar>& bytes);

} // namespace glcm

#endif // GLCM_IMAGE_HEADER_HPP_
