#ifndef GLCM_IMAGE_HEADER_HPP_
#define GLCM_IMAGE_HEADER_HPP_

#include <cstdint>
#include <opencv2/core.hpp>
#include <optional>
#include <string>
#include <vector>

namespace glcm {

// Physical size of one pixel in millimetres, horizontally (x) and vertically (y)
struct PixelSpacing {
    double x_mm = 0;
    double y_mm = 0;

    bool operator==(const PixelSpacing& other) const {
        return x_mm == other.x_mm && y_mm == other.y_mm;
    }
};

// Width and height declared by an image header. 64-bit, so absurd sizes in crafted files are reported as they are.
struct ImageSize {
    int64_t width = 0;
    int64_t height = 0;
    // TIFF only: the file holds more than one image (page); decoders read the first
    bool more_images = false;
    // From the resolution stored in the file: PNG pHYs (per metre), JPEG JFIF density (per inch or cm), BMP pixels per
    // metre, TIFF XResolution/YResolution with ResolutionUnit (inch or cm). Absent without a physical unit, and for
    // 72 or 96 pixels per inch on both axes: the defaults of image editors and screens, not a measurement.
    // A malformed resolution is ignored; it never makes the size unreadable.
    std::optional<PixelSpacing> pixel_spacing;
};

// Spacing for densities in pixels per unit, where one unit is mm_per_unit millimetres; nullopt for non-positive or
// non-finite densities, implausible spacings and the 72/96 dpi defaults (see ImageSize::pixel_spacing)
std::optional<PixelSpacing> SpacingFromDensity(double x_per_unit, double y_per_unit, double mm_per_unit);

// Reads the size from the header of a PNG, JPEG, BMP or TIFF (including BigTIFF) file without decoding the pixels, so
// the size can be checked before a decoder allocates memory for it.
// Returns nullopt for other formats. Throws std::runtime_error if the file cannot be read, or if the header of a
// recognized format is truncated or invalid.
std::optional<ImageSize> ReadImageSize(const std::string& path);

// Same as ReadImageSize, for an encoded image held in memory
std::optional<ImageSize> ReadImageSizeFromBytes(const std::vector<uchar>& bytes);

} // namespace glcm

#endif // GLCM_IMAGE_HEADER_HPP_
