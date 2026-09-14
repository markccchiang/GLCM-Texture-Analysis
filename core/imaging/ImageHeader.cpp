#include "imaging/ImageHeader.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <stdexcept>

namespace glcm {

namespace {

// Random access to the encoded bytes; Read returns the number of bytes available at the offset (fewer at the end)
class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual size_t Read(uint64_t offset, uint8_t* out, size_t count) = 0;
};

class FileSource : public ByteSource {
public:
    explicit FileSource(const std::string& path) : _stream(path, std::ios::binary) {
        if (!_stream) {
            throw std::runtime_error("Cannot read the image: " + path);
        }
    }

    size_t Read(uint64_t offset, uint8_t* out, size_t count) override {
        _stream.clear();
        _stream.seekg(static_cast<std::streamoff>(offset));
        if (!_stream) {
            return 0;
        }
        _stream.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(count));
        return static_cast<size_t>(_stream.gcount());
    }

private:
    std::ifstream _stream;
};

class MemorySource : public ByteSource {
public:
    explicit MemorySource(const std::vector<uchar>& bytes) : _bytes(bytes) {}

    size_t Read(uint64_t offset, uint8_t* out, size_t count) override {
        if (offset >= _bytes.size()) {
            return 0;
        }
        const size_t available = std::min(count, static_cast<size_t>(_bytes.size() - offset));
        std::memcpy(out, _bytes.data() + offset, available);
        return available;
    }

private:
    const std::vector<uchar>& _bytes;
};

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
    return {width, height};
}

ImageSize PngSize(ByteSource& source) {
    HeaderReader reader(source, "PNG");
    std::array<uint8_t, 4> chunk_type{};
    reader.Bytes(12, chunk_type.data(), chunk_type.size());
    if (std::memcmp(chunk_type.data(), "IHDR", 4) != 0) {
        reader.Invalid("the first chunk is not IHDR");
    }
    return Checked(reader, static_cast<int64_t>(reader.Unsigned(16, 4, false)), static_cast<int64_t>(reader.Unsigned(20, 4, false)));
}

bool IsJpegFrameMarker(uint8_t marker) {
    // SOF0-SOF15, except DHT (C4), JPG (C8) and DAC (CC)
    return marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
}

ImageSize JpegSize(ByteSource& source) {
    HeaderReader reader(source, "JPEG");
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
            return Checked(reader, width, height);
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
    return Checked(reader, width, height == INT32_MIN ? 0 : std::abs(static_cast<int64_t>(height)));
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
    return Checked(reader, width, height);
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

std::optional<ImageSize> ReadImageSize(const std::string& path) {
    FileSource source(path);
    return SizeFromSignature(source);
}

std::optional<ImageSize> ReadImageSizeFromBytes(const std::vector<uchar>& bytes) {
    MemorySource source(bytes);
    return SizeFromSignature(source);
}

} // namespace glcm
