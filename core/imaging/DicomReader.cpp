#include "imaging/DicomReader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <locale>
#include <map>
#include <opencv2/imgproc.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "imaging/ByteSource.hpp"
#include "imaging/ValueConversion.hpp"

namespace glcm {

namespace {

using imaging_detail::ByteSource;
using imaging_detail::FileSource;
using imaging_detail::MemorySource;

constexpr uint64_t PREAMBLE_SIZE = 128;
constexpr uint32_t UNDEFINED_LENGTH = 0xFFFFFFFF;
// Limits against crafted files
constexpr int MAX_SEQUENCE_DEPTH = 32;
constexpr uint64_t MAX_ELEMENTS = 1000000;
constexpr uint32_t MAX_STORED_VALUE_LENGTH = 1024;

constexpr uint32_t TAG_TRANSFER_SYNTAX = 0x00020010;
constexpr uint32_t TAG_MODALITY = 0x00080060;
constexpr uint32_t TAG_IMAGER_PIXEL_SPACING = 0x00181164;
constexpr uint32_t TAG_SAMPLES_PER_PIXEL = 0x00280002;
constexpr uint32_t TAG_PHOTOMETRIC = 0x00280004;
constexpr uint32_t TAG_PLANAR_CONFIGURATION = 0x00280006;
constexpr uint32_t TAG_NUMBER_OF_FRAMES = 0x00280008;
constexpr uint32_t TAG_ROWS = 0x00280010;
constexpr uint32_t TAG_COLUMNS = 0x00280011;
constexpr uint32_t TAG_PIXEL_SPACING = 0x00280030;
constexpr uint32_t TAG_BITS_ALLOCATED = 0x00280100;
constexpr uint32_t TAG_BITS_STORED = 0x00280101;
constexpr uint32_t TAG_HIGH_BIT = 0x00280102;
constexpr uint32_t TAG_PIXEL_REPRESENTATION = 0x00280103;
constexpr uint32_t TAG_WINDOW_CENTER = 0x00281050;
constexpr uint32_t TAG_WINDOW_WIDTH = 0x00281051;
constexpr uint32_t TAG_RESCALE_INTERCEPT = 0x00281052;
constexpr uint32_t TAG_RESCALE_SLOPE = 0x00281053;
constexpr uint32_t TAG_RESCALE_TYPE = 0x00281054;
constexpr uint32_t TAG_PIXEL_DATA = 0x7FE00010;
constexpr uint32_t TAG_ITEM = 0xFFFEE000;
constexpr uint32_t TAG_ITEM_END = 0xFFFEE00D;
constexpr uint32_t TAG_SEQUENCE_END = 0xFFFEE0DD;

constexpr std::array<uint32_t, 19> WANTED_TAGS = {TAG_TRANSFER_SYNTAX, TAG_MODALITY, TAG_IMAGER_PIXEL_SPACING, TAG_SAMPLES_PER_PIXEL,
    TAG_PHOTOMETRIC, TAG_PLANAR_CONFIGURATION, TAG_NUMBER_OF_FRAMES, TAG_ROWS, TAG_COLUMNS, TAG_PIXEL_SPACING, TAG_BITS_ALLOCATED,
    TAG_BITS_STORED, TAG_HIGH_BIT, TAG_PIXEL_REPRESENTATION, TAG_WINDOW_CENTER, TAG_WINDOW_WIDTH, TAG_RESCALE_INTERCEPT, TAG_RESCALE_SLOPE,
    TAG_RESCALE_TYPE};

// Value representations with a 2-byte reserved field and a 4-byte length in explicit VR
bool HasLongLength(const std::array<char, 2>& vr) {
    static const std::array<const char*, 13> LONG_VRS = {"OB", "OD", "OF", "OL", "OV", "OW", "SQ", "SV", "UC", "UN", "UR", "UT", "UV"};
    return std::any_of(LONG_VRS.begin(), LONG_VRS.end(), [&](const char* name) { return vr[0] == name[0] && vr[1] == name[1]; });
}

struct ElementHeader {
    uint32_t tag = 0;
    std::array<char, 2> vr{};
    bool has_vr = false;
    uint32_t length = 0;
    uint64_t value_offset = 0;
};

// The attributes of the top-level dataset that the reader uses, as raw values
struct Attributes {
    std::map<uint32_t, std::string> values;
    bool has_pixel_data = false;
    uint64_t pixel_offset = 0;
    uint32_t pixel_length = 0;
};

class DicomParser {
public:
    explicit DicomParser(ByteSource& source) : _source(source) {}

    Attributes Parse() {
        // File meta information: group 0002, always explicit VR little endian
        uint64_t position = PREAMBLE_SIZE + 4;
        while (Available(position, 8)) {
            const ElementHeader header = ReadHeader(position, true);
            if ((header.tag >> 16) != 0x0002) {
                break;
            }
            if (header.length == UNDEFINED_LENGTH) {
                throw std::runtime_error("Invalid DICOM file meta information");
            }
            Store(header);
            position = header.value_offset + header.length;
        }

        const std::string syntax = Text(TAG_TRANSFER_SYNTAX);
        bool explicit_vr = true;
        if (syntax.empty() || syntax == "1.2.840.10008.1.2") {
            explicit_vr = false; // implicit VR little endian, also assumed when the transfer syntax is missing
        } else if (syntax == "1.2.840.10008.1.2.1") {
            explicit_vr = true;
        } else if (syntax == "1.2.840.10008.1.2.2") {
            throw std::invalid_argument("Big-endian DICOM files are not supported; convert the file to little endian");
        } else if (syntax == "1.2.840.10008.1.2.1.99") {
            throw std::invalid_argument("Deflated DICOM files are not supported; convert the file to an uncompressed transfer syntax");
        } else if (syntax.rfind("1.2.840.10008.1.2.4.", 0) == 0 || syntax == "1.2.840.10008.1.2.5") {
            throw std::invalid_argument(
                "Compressed DICOM images (JPEG, JPEG-LS, JPEG 2000 or RLE) are not supported; convert the file "
                "to an uncompressed transfer syntax, e.g. with dcmdjpeg or gdcmconv --raw");
        } else {
            throw std::invalid_argument("The DICOM transfer syntax " + syntax +
                                        " is not supported; convert the file to an uncompressed "
                                        "transfer syntax");
        }
        ParseElements(position, explicit_vr, 0, true);
        return _attributes;
    }

    std::string Text(uint32_t tag) const {
        const auto found = _attributes.values.find(tag);
        if (found == _attributes.values.end()) {
            return "";
        }
        std::string text = found->second;
        // Values are padded with spaces (text) or NUL (UI) to an even length
        const size_t end = text.find_last_not_of(std::string(" \0", 2));
        text.erase(end == std::string::npos ? 0 : end + 1);
        const size_t begin = text.find_first_not_of(' ');
        return begin == std::string::npos ? "" : text.substr(begin);
    }

    std::optional<int> UnsignedShort(uint32_t tag) const {
        const auto found = _attributes.values.find(tag);
        if (found == _attributes.values.end() || found->second.size() < 2) {
            return std::nullopt;
        }
        const auto* bytes = reinterpret_cast<const unsigned char*>(found->second.data());
        return bytes[0] | (bytes[1] << 8);
    }

private:
    bool Available(uint64_t offset, size_t count) {
        std::array<uint8_t, 16> buffer{};
        return _source.Read(offset, buffer.data(), std::min(count, buffer.size())) == std::min(count, buffer.size());
    }

    void Bytes(uint64_t offset, uint8_t* out, size_t count) {
        if (_source.Read(offset, out, count) != count) {
            throw std::runtime_error("Truncated DICOM file");
        }
    }

    uint16_t Uint16(uint64_t offset) {
        std::array<uint8_t, 2> bytes{};
        Bytes(offset, bytes.data(), bytes.size());
        return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
    }

    uint32_t Uint32(uint64_t offset) {
        std::array<uint8_t, 4> bytes{};
        Bytes(offset, bytes.data(), bytes.size());
        return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) | (static_cast<uint32_t>(bytes[2]) << 16) |
               (static_cast<uint32_t>(bytes[3]) << 24);
    }

    ElementHeader ReadHeader(uint64_t position, bool explicit_vr) {
        ElementHeader header;
        header.tag = (static_cast<uint32_t>(Uint16(position)) << 16) | Uint16(position + 2);
        if (!explicit_vr || (header.tag >> 16) == 0xFFFE) {
            header.length = Uint32(position + 4);
            header.value_offset = position + 8;
            return header;
        }
        std::array<uint8_t, 2> vr{};
        Bytes(position + 4, vr.data(), vr.size());
        header.vr = {static_cast<char>(vr[0]), static_cast<char>(vr[1])};
        header.has_vr = true;
        if (HasLongLength(header.vr)) {
            header.length = Uint32(position + 8);
            header.value_offset = position + 12;
        } else {
            header.length = Uint16(position + 6);
            header.value_offset = position + 8;
        }
        return header;
    }

    void CountElement() {
        if (++_elements > MAX_ELEMENTS) {
            throw std::runtime_error("Invalid DICOM file: too many elements");
        }
    }

    void Store(const ElementHeader& header) {
        if (std::find(WANTED_TAGS.begin(), WANTED_TAGS.end(), header.tag) == WANTED_TAGS.end() || header.length > MAX_STORED_VALUE_LENGTH) {
            return;
        }
        std::string value(header.length, '\0');
        Bytes(header.value_offset, reinterpret_cast<uint8_t*>(value.data()), value.size());
        _attributes.values[header.tag] = std::move(value);
    }

    // Reads the elements of a dataset. At the top level it ends at the pixel data or the end of the file; in an item of
    // undefined length, after the item delimiter. Returns the position after the last element read.
    uint64_t ParseElements(uint64_t position, bool explicit_vr, int depth, bool top_level) {
        while (true) {
            if (top_level && !Available(position, 1)) {
                return position;
            }
            CountElement();
            const ElementHeader header = ReadHeader(position, explicit_vr);
            if (!top_level && header.tag == TAG_ITEM_END) {
                return header.value_offset;
            }
            if (top_level && header.tag == TAG_PIXEL_DATA) {
                if (header.length == UNDEFINED_LENGTH) {
                    throw std::invalid_argument(
                        "Compressed (encapsulated) DICOM pixel data is not supported; convert the file to an "
                        "uncompressed transfer syntax");
                }
                _attributes.has_pixel_data = true;
                _attributes.pixel_offset = header.value_offset;
                _attributes.pixel_length = header.length;
                return header.value_offset;
            }
            if (header.length == UNDEFINED_LENGTH) {
                // A sequence; the items of UN elements are encoded in implicit VR
                const bool unknown = header.has_vr && header.vr[0] == 'U' && header.vr[1] == 'N';
                position = SkipSequence(header.value_offset, explicit_vr && !unknown, depth + 1);
            } else {
                if (top_level) {
                    Store(header);
                }
                position = header.value_offset + header.length;
            }
        }
    }

    uint64_t SkipSequence(uint64_t position, bool explicit_vr, int depth) {
        if (depth > MAX_SEQUENCE_DEPTH) {
            throw std::runtime_error("Invalid DICOM file: sequences nested too deeply");
        }
        while (true) {
            CountElement();
            const uint32_t tag = (static_cast<uint32_t>(Uint16(position)) << 16) | Uint16(position + 2);
            const uint32_t length = Uint32(position + 4);
            position += 8;
            if (tag == TAG_SEQUENCE_END) {
                return position;
            }
            if (tag != TAG_ITEM) {
                throw std::runtime_error("Invalid DICOM sequence");
            }
            position = length == UNDEFINED_LENGTH ? ParseElements(position, explicit_vr, depth, false) : position + length;
        }
    }

    ByteSource& _source;
    Attributes _attributes;
    uint64_t _elements = 0;
};

std::vector<double> Numbers(const std::string& text) {
    std::vector<double> numbers;
    std::stringstream parts(text);
    std::string part;
    while (std::getline(parts, part, '\\')) {
        std::istringstream in(part);
        in.imbue(std::locale::classic());
        double number = 0;
        if (in >> number && std::isfinite(number)) {
            numbers.push_back(number);
        } else {
            numbers.push_back(std::numeric_limits<double>::quiet_NaN());
        }
    }
    return numbers;
}

std::optional<double> FirstNumber(const std::string& text) {
    const std::vector<double> numbers = Numbers(text);
    if (numbers.empty() || !std::isfinite(numbers[0])) {
        return std::nullopt;
    }
    return numbers[0];
}

std::optional<PixelSpacing> Spacing(const std::string& text) {
    const std::vector<double> numbers = Numbers(text);
    // Row spacing (between rows: vertical), then column spacing (horizontal)
    if (numbers.size() != 2) {
        return std::nullopt;
    }
    const auto plausible = [](double mm) { return std::isfinite(mm) && mm >= 1e-6 && mm <= 1e6; };
    if (!plausible(numbers[0]) || !plausible(numbers[1])) {
        return std::nullopt;
    }
    return PixelSpacing{numbers[1], numbers[0]};
}

bool IsInteger(double value) {
    return std::isfinite(value) && std::abs(value - std::round(value)) < 1e-9;
}

std::string Capitalized(std::string text) {
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') {
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    }
    return text;
}

int RequiredShort(const DicomParser& parser, uint32_t tag, const char* name) {
    const std::optional<int> value = parser.UnsignedShort(tag);
    if (!value) {
        throw std::runtime_error(std::string("Invalid DICOM image: ") + name + " is missing");
    }
    return *value;
}

LoadedImage LoadDicom(ByteSource& source, int64_t max_pixels) {
    std::array<uint8_t, 4> magic{};
    if (source.Read(PREAMBLE_SIZE, magic.data(), magic.size()) != magic.size() || magic != std::array<uint8_t, 4>{'D', 'I', 'C', 'M'}) {
        throw std::runtime_error("Not a DICOM file");
    }
    DicomParser parser(source);
    const Attributes attributes = parser.Parse();
    if (!attributes.has_pixel_data) {
        throw std::invalid_argument("The DICOM file contains no image (pixel data)");
    }

    const int64_t rows = RequiredShort(parser, TAG_ROWS, "Rows");
    const int64_t columns = RequiredShort(parser, TAG_COLUMNS, "Columns");
    const int bits_allocated = RequiredShort(parser, TAG_BITS_ALLOCATED, "BitsAllocated");
    const int samples_per_pixel = parser.UnsignedShort(TAG_SAMPLES_PER_PIXEL).value_or(1);
    const int bits_stored = parser.UnsignedShort(TAG_BITS_STORED).value_or(bits_allocated);
    const int high_bit = parser.UnsignedShort(TAG_HIGH_BIT).value_or(bits_stored - 1);
    const bool is_signed = parser.UnsignedShort(TAG_PIXEL_REPRESENTATION).value_or(0) == 1;
    const std::string photometric = parser.Text(TAG_PHOTOMETRIC);
    if (rows <= 0 || columns <= 0) {
        throw std::runtime_error("Invalid DICOM image: the rows and columns must be positive");
    }
    if (bits_allocated != 8 && bits_allocated != 16) {
        throw std::invalid_argument(
            "DICOM images with " + std::to_string(bits_allocated) + " bits allocated are not supported; only 8 and 16 bits are");
    }
    if (bits_stored < 1 || bits_stored > bits_allocated || high_bit < bits_stored - 1 || high_bit >= bits_allocated) {
        throw std::runtime_error("Invalid DICOM image: inconsistent BitsStored and HighBit");
    }
    const bool monochrome = photometric == "MONOCHROME1" || photometric == "MONOCHROME2";
    if (!(samples_per_pixel == 1 && monochrome) && !(samples_per_pixel == 3 && photometric == "RGB")) {
        throw std::invalid_argument("DICOM images with the photometric interpretation " + (photometric.empty() ? "(none)" : photometric) +
                                    " and " + std::to_string(samples_per_pixel) +
                                    " samples per pixel are not supported; only MONOCHROME1, MONOCHROME2 and RGB are");
    }
    if (max_pixels > 0 && columns > max_pixels / rows) {
        throw ImageTooLargeError(columns, rows, max_pixels);
    }

    LoadedImage image;
    const std::optional<double> frames = FirstNumber(parser.Text(TAG_NUMBER_OF_FRAMES));
    if (frames && *frames > 1) {
        image.warnings.push_back("The DICOM file contains " + FormatValue(*frames) + " frames; only the first is used");
    }

    const uint64_t bytes_per_sample = bits_allocated / 8;
    const uint64_t pixel_count = static_cast<uint64_t>(rows) * static_cast<uint64_t>(columns);
    const uint64_t frame_bytes = pixel_count * samples_per_pixel * bytes_per_sample;
    uint8_t last = 0;
    if (attributes.pixel_length < frame_bytes || source.Read(attributes.pixel_offset + frame_bytes - 1, &last, 1) != 1) {
        throw std::runtime_error("Truncated DICOM pixel data");
    }
    std::vector<uint8_t> bytes(frame_bytes);
    if (source.Read(attributes.pixel_offset, bytes.data(), bytes.size()) != bytes.size()) {
        throw std::runtime_error("Truncated DICOM pixel data");
    }

    const int shift = high_bit + 1 - bits_stored;
    const uint32_t mask = bits_stored >= 32 ? 0xFFFFFFFFu : ((1u << bits_stored) - 1);
    const auto sample = [&](uint64_t index) -> int32_t {
        uint32_t word = bytes_per_sample == 1 ? bytes[index] : static_cast<uint32_t>(bytes[2 * index] | (bytes[2 * index + 1] << 8));
        word = (word >> shift) & mask;
        if (is_signed && (word & (1u << (bits_stored - 1)))) {
            return static_cast<int32_t>(word) - (1 << bits_stored);
        }
        return static_cast<int32_t>(word);
    };

    image.info.width = static_cast<int>(columns);
    image.info.height = static_cast<int>(rows);
    image.info.source_channels = samples_per_pixel;
    const std::string spacing_text = parser.Text(TAG_PIXEL_SPACING);
    image.info.pixel_spacing = Spacing(spacing_text);
    if (!image.info.pixel_spacing) {
        image.info.pixel_spacing = Spacing(parser.Text(TAG_IMAGER_PIXEL_SPACING));
    }

    if (samples_per_pixel == 3) {
        const bool planar = parser.UnsignedShort(TAG_PLANAR_CONFIGURATION).value_or(0) == 1;
        const int depth = bits_allocated == 8 ? CV_8U : CV_16U;
        cv::Mat rgb(static_cast<int>(rows), static_cast<int>(columns), CV_MAKETYPE(depth, 3));
        for (uint64_t p = 0; p < pixel_count; ++p) {
            for (uint64_t c = 0; c < 3; ++c) {
                const uint64_t index = planar ? c * pixel_count + p : p * 3 + c;
                const int value = sample(index);
                const int r = static_cast<int>(p / columns);
                const int x = static_cast<int>(p % columns);
                if (depth == CV_8U) {
                    rgb.at<cv::Vec3b>(r, x)[static_cast<int>(c)] = static_cast<uchar>(value);
                } else {
                    rgb.at<cv::Vec3w>(r, x)[static_cast<int>(c)] = static_cast<uint16_t>(value);
                }
            }
        }
        cv::cvtColor(rgb, image.gray, cv::COLOR_RGB2GRAY);
        image.info.bit_depth = bits_allocated;
        image.warnings.push_back("Color image converted to grayscale");
        return image;
    }

    std::optional<double> slope = FirstNumber(parser.Text(TAG_RESCALE_SLOPE));
    if (!slope || *slope == 0) {
        slope = 1;
    }
    const double intercept = FirstNumber(parser.Text(TAG_RESCALE_INTERCEPT)).value_or(0);
    const std::string rescale_type = parser.Text(TAG_RESCALE_TYPE);
    const std::string unit = (rescale_type == "HU" || (rescale_type.empty() && parser.Text(TAG_MODALITY) == "CT")) ? "HU" : "";
    const bool inverted = photometric == "MONOCHROME1";
    const int32_t max_sample = (1 << (is_signed ? bits_stored - 1 : bits_stored)) - 1;

    std::vector<int32_t> samples(pixel_count);
    int32_t lowest = std::numeric_limits<int32_t>::max();
    int32_t highest = std::numeric_limits<int32_t>::min();
    for (uint64_t p = 0; p < pixel_count; ++p) {
        int32_t value = sample(p);
        if (inverted) {
            // Unsigned: max - value; signed: -1 - value. Both reverse the range of the stored bits onto itself.
            value = is_signed ? -1 - value : max_sample - value;
        }
        samples[p] = value;
        lowest = std::min(lowest, value);
        highest = std::max(highest, value);
    }
    const double value_a = lowest * *slope + intercept;
    const double value_b = highest * *slope + intercept;
    ValueRange range{std::min(value_a, value_b), std::max(value_a, value_b), IsInteger(*slope) && IsInteger(intercept)};
    const StorageChoice storage = ChooseStorage(range, bits_allocated == 8 && !is_signed, unit == "HU");

    image.gray = cv::Mat(static_cast<int>(rows), static_cast<int>(columns), storage.bit_depth == 8 ? CV_8UC1 : CV_16UC1);
    uint64_t clipped = 0;
    for (uint64_t p = 0; p < pixel_count; ++p) {
        const double value = samples[p] * *slope + intercept;
        const int stored = StoredSample(value, storage);
        if (storage.kind == StorageKind::Offset && value < -STORAGE_OFFSET) {
            ++clipped;
        }
        const int r = static_cast<int>(p / columns);
        const int x = static_cast<int>(p % columns);
        if (storage.bit_depth == 8) {
            image.gray.at<uchar>(r, x) = static_cast<uchar>(stored);
        } else {
            image.gray.at<uint16_t>(r, x) = static_cast<uint16_t>(stored);
        }
    }
    image.info.bit_depth = storage.bit_depth;
    if (clipped > 0) {
        image.warnings.push_back(std::to_string(clipped) + " pixels below -1024" + (unit.empty() ? "" : " " + unit) + " are stored as 0");
    }

    // The file's value in terms of the stored sample. Inverted: value = K - (stored × scale + offset), with K the value of
    // the inverted sample plus the value of the original sample (the same for every sample)
    double scale = storage.scale;
    double offset = storage.offset;
    if (inverted) {
        const double k = (is_signed ? -1.0 : static_cast<double>(max_sample)) * *slope + 2 * intercept;
        scale = -storage.scale;
        offset = k - storage.offset;
    }
    const bool rescaled = *slope != 1 || intercept != 0;
    if (inverted || rescaled || storage.kind != StorageKind::Identity) {
        std::vector<std::string> parts;
        if (inverted) {
            parts.push_back("MONOCHROME1 inverted so that bright means dense");
        }
        if (rescaled) {
            parts.push_back("rescale slope " + FormatValue(*slope) + ", intercept " + FormatValue(intercept));
        }
        if (storage.kind == StorageKind::Offset) {
            parts.push_back("values stored + 1024");
        } else if (storage.kind == StorageKind::Linear) {
            parts.push_back(
                "values mapped linearly from " + FormatValue(range.minimum) + " – " + FormatValue(range.maximum) + " to 0 – 65535");
        }
        parts.push_back(ConversionFormula(scale, offset, unit));
        std::string description;
        for (const std::string& part : parts) {
            description += (description.empty() ? "" : "; ") + part;
        }
        image.info.value_conversion = ValueConversion{scale, offset, unit, Capitalized(description)};
    }

    const std::optional<double> center = FirstNumber(parser.Text(TAG_WINDOW_CENTER));
    const std::optional<double> width = FirstNumber(parser.Text(TAG_WINDOW_WIDTH));
    if (center && width && *width >= 1) {
        // DICOM linear window: from c - 0.5 - (w - 1) / 2 to c - 0.5 + (w - 1) / 2, in the file's values
        const double max_stored = storage.bit_depth == 8 ? 255 : 65535;
        const auto stored = [&](double value) { return std::clamp(std::round((value - offset) / scale), 0.0, max_stored); };
        double low = stored(*center - 0.5 - (*width - 1) / 2);
        double high = stored(*center - 0.5 + (*width - 1) / 2);
        if (low > high) {
            std::swap(low, high);
        }
        image.window = DisplayWindow{static_cast<int>(low), static_cast<int>(high)};
    }
    return image;
}

bool HasDicomMagic(ByteSource& source) {
    std::array<uint8_t, 4> magic{};
    return source.Read(PREAMBLE_SIZE, magic.data(), magic.size()) == magic.size() && magic == std::array<uint8_t, 4>{'D', 'I', 'C', 'M'};
}

} // namespace

bool IsDicomFile(const std::string& path) {
    FileSource source(path);
    return HasDicomMagic(source);
}

bool IsDicomBytes(const std::vector<uchar>& bytes) {
    MemorySource source(bytes);
    return HasDicomMagic(source);
}

LoadedImage LoadDicomFile(const std::string& path, int64_t max_pixels) {
    FileSource source(path);
    return LoadDicom(source, max_pixels);
}

LoadedImage LoadDicomBytes(const std::vector<uchar>& bytes, int64_t max_pixels) {
    MemorySource source(bytes);
    return LoadDicom(source, max_pixels);
}

} // namespace glcm
