#include "imaging/ImageHeader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <stdexcept>

#include "imaging/ByteSource.hpp"

namespace glcm {

namespace {

using imaging_detail::ByteSource;
using imaging_detail::FileSource;
using imaging_detail::MemorySource;

// Largest TIFF directory read; real files have a few dozen entries
constexpr uint64_t MAX_TIFF_ENTRIES = 65536;

class HeaderReader {
public:
    HeaderReader(ByteSource& source, const char* format) : _source(source), _format(format) {}

    void Bytes(uint64_t offset, uint8_t* out, size_t count) {
        if (_source.Read(offset, out, count) != count) {
            throw std::runtime_error(std::string("Truncated ") + _format + " header");
        }
    }

    uint64_t Unsigned(uint64_t offset, size_t count, bool little_endian) {
        std::array<uint8_t, 8> buffer{};
        Bytes(offset, buffer.data(), count);
        uint64_t value = 0;
        for (size_t i = 0; i < count; ++i) {
            const uint64_t byte = buffer[little_endian ? count - 1 - i : i];
            value = (value << 8) | byte;
        }
        return value;
    }

    [[noreturn]] void Invalid(const std::string& reason) const {
        throw std::runtime_error(std::string("Invalid ") + _format + " header: " + reason);
    }

private:
    ByteSource& _source;
    const char* _format;
};

ImageSize Checked(HeaderReader& reader, int64_t width, int64_t height) {
    if (width <= 0 || height <= 0) {
        reader.Invalid("the width and height must be positive");
    }
    ImageSize size;
    size.width = width;
    size.height = height;
    return size;
}

constexpr double MM_PER_INCH = 25.4;
constexpr double MM_PER_CM = 10.0;
constexpr double MM_PER_METRE = 1000.0;
// Chunks or directory entries looked at for the resolution, so a crafted file cannot keep the reader busy
constexpr int MAX_METADATA_ITEMS = 4096;

// Runs a metadata reader; a truncated or malformed resolution only means that the spacing is unknown
template <typename Read>
std::optional<PixelSpacing> OptionalSpacing(Read read) {
    try {
        return read();
    } catch (const std::runtime_error&) {
        return std::nullopt;
    }
}

// pHYs may appear anywhere between IHDR and the first IDAT chunk
std::optional<PixelSpacing> PngSpacing(HeaderReader& reader) {
    uint64_t offset = 8;
    for (int i = 0; i < MAX_METADATA_ITEMS; ++i) {
        const uint64_t length = reader.Unsigned(offset, 4, false);
        std::array<uint8_t, 4> type{};
        reader.Bytes(offset + 4, type.data(), type.size());
        if (std::memcmp(type.data(), "IDAT", 4) == 0 || std::memcmp(type.data(), "IEND", 4) == 0) {
            return std::nullopt;
        }
        if (std::memcmp(type.data(), "pHYs", 4) == 0 && length == 9) {
            // Pixels per unit on x and y, then the unit: 1 = metre, 0 = aspect ratio only
            if (reader.Unsigned(offset + 16, 1, false) != 1) {
                return std::nullopt;
            }
            return SpacingFromDensity(static_cast<double>(reader.Unsigned(offset + 8, 4, false)),
                static_cast<double>(reader.Unsigned(offset + 12, 4, false)), MM_PER_METRE);
        }
        offset += 12 + length; // length, type, data, CRC
    }
    return std::nullopt;
}

ImageSize PngSize(ByteSource& source) {
    HeaderReader reader(source, "PNG");
    std::array<uint8_t, 4> chunk_type{};
    reader.Bytes(12, chunk_type.data(), chunk_type.size());
    if (std::memcmp(chunk_type.data(), "IHDR", 4) != 0) {
        reader.Invalid("the first chunk is not IHDR");
    }
    ImageSize size = Checked(reader, static_cast<int64_t>(reader.Unsigned(16, 4, false)), static_cast<int64_t>(reader.Unsigned(20, 4, false)));
    size.pixel_spacing = OptionalSpacing([&] { return PngSpacing(reader); });
    return size;
}

bool IsJpegFrameMarker(uint8_t marker) {
    // SOF0-SOF15, except DHT (C4), JPG (C8) and DAC (CC)
    return marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
}

// JFIF APP0 segment: length (2), "JFIF\0" (5), version (2), units (1: per inch, 2: per cm, 0: aspect ratio only), X and
// Y density (2 each)
std::optional<PixelSpacing> JfifSpacing(HeaderReader& reader, uint64_t offset, uint64_t length) {
    std::array<uint8_t, 5> identifier{};
    if (length < 16) {
        return std::nullopt;
    }
    reader.Bytes(offset + 2, identifier.data(), identifier.size());
    if (std::memcmp(identifier.data(), "JFIF\0", 5) != 0) {
        return std::nullopt;
    }
    const uint64_t units = reader.Unsigned(offset + 9, 1, false);
    if (units != 1 && units != 2) {
        return std::nullopt;
    }
    return SpacingFromDensity(static_cast<double>(reader.Unsigned(offset + 10, 2, false)),
        static_cast<double>(reader.Unsigned(offset + 12, 2, false)), units == 1 ? MM_PER_INCH : MM_PER_CM);
}

ImageSize JpegSize(ByteSource& source) {
    HeaderReader reader(source, "JPEG");
    std::optional<PixelSpacing> spacing;
    uint64_t offset = 2; // after SOI
    for (;;) {
        if (reader.Unsigned(offset, 1, false) != 0xFF) {
            reader.Invalid("expected a marker");
        }
        // Markers may be preceded by any number of fill bytes 0xFF
        uint8_t marker = 0xFF;
        while (marker == 0xFF) {
            offset += 1;
            marker = static_cast<uint8_t>(reader.Unsigned(offset, 1, false));
        }
        offset += 1;
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8)) {
            continue; // markers without a length
        }
        if (marker == 0xD9 || marker == 0xDA) {
            reader.Invalid("no frame header before the image data");
        }
        const uint64_t length = reader.Unsigned(offset, 2, false);
        if (length < 2) {
            reader.Invalid("segment length below 2");
        }
        if (IsJpegFrameMarker(marker)) {
            // Segment: length (2), sample precision (1), height (2), width (2)
            const auto height = static_cast<int64_t>(reader.Unsigned(offset + 3, 2, false));
            const auto width = static_cast<int64_t>(reader.Unsigned(offset + 5, 2, false));
            ImageSize size = Checked(reader, width, height);
            size.pixel_spacing = spacing;
            return size;
        }
        if (marker == 0xE0 && !spacing) {
            spacing = OptionalSpacing([&] { return JfifSpacing(reader, offset, length); });
        }
        offset += length;
    }
}

ImageSize BmpSize(ByteSource& source) {
    HeaderReader reader(source, "BMP");
    const uint64_t header_size = reader.Unsigned(14, 4, true);
    if (header_size == 12) {
        // OS/2 BITMAPCOREHEADER: 16-bit width and height
        return Checked(reader, static_cast<int64_t>(reader.Unsigned(18, 2, true)), static_cast<int64_t>(reader.Unsigned(20, 2, true)));
    }
    if (header_size < 40) {
        reader.Invalid("unknown DIB header size");
    }
    // BITMAPINFOHEADER and later: signed 32-bit width and height; a negative height means top-down rows
    const auto width = static_cast<int32_t>(static_cast<uint32_t>(reader.Unsigned(18, 4, true)));
    const auto height = static_cast<int32_t>(static_cast<uint32_t>(reader.Unsigned(22, 4, true)));
    ImageSize size = Checked(reader, width, height == INT32_MIN ? 0 : std::abs(static_cast<int64_t>(height)));
    // Signed 32-bit horizontal and vertical pixels per metre; 0 when unknown
    size.pixel_spacing = OptionalSpacing([&] {
        const auto x = static_cast<int32_t>(static_cast<uint32_t>(reader.Unsigned(38, 4, true)));
        const auto y = static_cast<int32_t>(static_cast<uint32_t>(reader.Unsigned(42, 4, true)));
        return SpacingFromDensity(x, y, MM_PER_METRE);
    });
    return size;
}

// XResolution (282) and YResolution (283) are RATIONALs in pixels per ResolutionUnit (296: 1 none, 2 inch (default),
// 3 cm)
std::optional<PixelSpacing> TiffSpacing(HeaderReader& reader, bool little_endian, bool big, uint64_t first_entry, uint64_t entries) {
    const uint64_t entry_size = big ? 20 : 12;
    const uint64_t value_offset = big ? 12 : 8;
    std::optional<double> x_resolution;
    std::optional<double> y_resolution;
    uint64_t unit = 2;
    const auto rational = [&](uint64_t entry) -> std::optional<double> {
        if (reader.Unsigned(entry + 2, 2, little_endian) != 5) {
            return std::nullopt;
        }
        // Eight bytes: inline in a BigTIFF entry, elsewhere in a classic TIFF
        const uint64_t at = big ? entry + value_offset : reader.Unsigned(entry + value_offset, 4, little_endian);
        const uint64_t numerator = reader.Unsigned(at, 4, little_endian);
        const uint64_t denominator = reader.Unsigned(at + 4, 4, little_endian);
        if (denominator == 0) {
            return std::nullopt;
        }
        return static_cast<double>(numerator) / static_cast<double>(denominator);
    };
    for (uint64_t i = 0; i < std::min<uint64_t>(entries, MAX_METADATA_ITEMS); ++i) {
        const uint64_t entry = first_entry + i * entry_size;
        const uint64_t tag = reader.Unsigned(entry, 2, little_endian);
        if (tag == 282) {
            x_resolution = rational(entry);
        } else if (tag == 283) {
            y_resolution = rational(entry);
        } else if (tag == 296 && reader.Unsigned(entry + 2, 2, little_endian) == 3) {
            unit = reader.Unsigned(entry + value_offset, 2, little_endian);
        }
    }
    if (!x_resolution || !y_resolution || (unit != 2 && unit != 3)) {
        return std::nullopt;
    }
    return SpacingFromDensity(*x_resolution, *y_resolution, unit == 2 ? MM_PER_INCH : MM_PER_CM);
}

ImageSize TiffSize(ByteSource& source, bool little_endian) {
    HeaderReader reader(source, "TIFF");
    const uint64_t version = reader.Unsigned(2, 2, little_endian);
    const bool big = version == 43;
    if (!big && version != 42) {
        reader.Invalid("unknown version");
    }
    if (big && (reader.Unsigned(4, 2, little_endian) != 8 || reader.Unsigned(6, 2, little_endian) != 0)) {
        reader.Invalid("unsupported BigTIFF offset size");
    }

    // Image File Directory of the first image: entry count, then entries of tag, type, count and value
    const size_t offset_size = big ? 8 : 4;
    const uint64_t directory = reader.Unsigned(big ? 8 : 4, offset_size, little_endian);
    const uint64_t entries = reader.Unsigned(directory, big ? 8 : 2, little_endian);
    if (entries > MAX_TIFF_ENTRIES) {
        reader.Invalid("too many directory entries");
    }
    const uint64_t first_entry = directory + (big ? 8 : 2);
    const uint64_t entry_size = big ? 20 : 12;
    const uint64_t value_offset = big ? 12 : 8;

    int64_t width = -1;
    int64_t height = -1;
    for (uint64_t i = 0; i < entries && (width < 0 || height < 0); ++i) {
        const uint64_t entry = first_entry + i * entry_size;
        const uint64_t tag = reader.Unsigned(entry, 2, little_endian);
        if (tag != 256 && tag != 257) {
            continue;
        }
        const uint64_t type = reader.Unsigned(entry + 2, 2, little_endian);
        uint64_t value = 0;
        if (type == 3) { // SHORT
            value = reader.Unsigned(entry + value_offset, 2, little_endian);
        } else if (type == 4) { // LONG
            value = reader.Unsigned(entry + value_offset, 4, little_endian);
        } else if (type == 16 && big) { // LONG8
            value = reader.Unsigned(entry + value_offset, 8, little_endian);
        } else {
            reader.Invalid("unexpected type of the image width or length");
        }
        const auto dimension = static_cast<int64_t>(std::min<uint64_t>(value, INT64_MAX));
        (tag == 256 ? width : height) = dimension;
    }
    if (width < 0 || height < 0) {
        reader.Invalid("missing image width or length");
    }
    ImageSize size = Checked(reader, width, height);
    size.pixel_spacing = OptionalSpacing([&] { return TiffSpacing(reader, little_endian, big, first_entry, entries); });
    // The offset of the next image directory follows the entries; it is non-zero in a multi-page file
    try {
        size.more_images = reader.Unsigned(first_entry + entries * entry_size, offset_size, little_endian) != 0;
    } catch (const std::runtime_error&) {
        // A file that ends before the offset holds one image
    }
    return size;
}

std::optional<ImageSize> SizeFromSignature(ByteSource& source) {
    std::array<uint8_t, 8> signature{};
    const size_t available = source.Read(0, signature.data(), signature.size());
    const auto starts_with = [&](std::initializer_list<uint8_t> prefix) {
        return available >= prefix.size() && std::equal(prefix.begin(), prefix.end(), signature.begin());
    };

    if (starts_with({0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A})) {
        return PngSize(source);
    }
    if (starts_with({0xFF, 0xD8, 0xFF})) {
        return JpegSize(source);
    }
    if (starts_with({'B', 'M'})) {
        return BmpSize(source);
    }
    if (starts_with({'I', 'I'})) {
        return TiffSize(source, true);
    }
    if (starts_with({'M', 'M'})) {
        return TiffSize(source, false);
    }
    return std::nullopt;
}

} // namespace

std::optional<PixelSpacing> SpacingFromDensity(double x_per_unit, double y_per_unit, double mm_per_unit) {
    if (!std::isfinite(x_per_unit) || !std::isfinite(y_per_unit) || x_per_unit <= 0 || y_per_unit <= 0) {
        return std::nullopt;
    }
    const PixelSpacing spacing{mm_per_unit / x_per_unit, mm_per_unit / y_per_unit};
    // From 1 nm to 1 km per pixel
    const auto plausible = [](double mm) { return mm >= 1e-6 && mm <= 1e6; };
    if (!plausible(spacing.x_mm) || !plausible(spacing.y_mm)) {
        return std::nullopt;
    }
    const auto dpi_default = [&](double mm_per_pixel) {
        const double dpi = MM_PER_INCH / mm_per_pixel;
        return std::abs(dpi - 72) < 0.5 || std::abs(dpi - 96) < 0.5;
    };
    if (dpi_default(spacing.x_mm) && dpi_default(spacing.y_mm)) {
        return std::nullopt;
    }
    return spacing;
}

std::optional<ImageSize> ReadImageSize(const std::string& path) {
    FileSource source(path);
    return SizeFromSignature(source);
}

std::optional<ImageSize> ReadImageSizeFromBytes(const std::vector<uchar>& bytes) {
    MemorySource source(bytes);
    return SizeFromSignature(source);
}

} // namespace glcm
