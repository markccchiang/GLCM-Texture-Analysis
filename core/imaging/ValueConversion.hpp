#ifndef GLCM_VALUE_CONVERSION_HPP_
#define GLCM_VALUE_CONVERSION_HPP_

#include <string>

namespace glcm {

// How the values of a medical image (DICOM, NIfTI) became 8- or 16-bit samples:
// value = stored sample × scale + offset, where the value is the file's value after its rescale slope and intercept
struct ValueConversion {
    double scale = 1;
    double offset = 0;
    std::string unit;        // "HU" for CT; empty when the file does not say
    std::string description; // e.g. "Rescale slope 1, intercept -1024; values stored + 1024; HU = stored value - 1024"
};

// The values to store
struct ValueRange {
    double minimum = 0;
    double maximum = 0;
    bool integral = true; // every value is an integer: integer samples with an integer slope and intercept
};

enum class StorageKind {
    Identity, // integers within 0-65535, unchanged
    Offset,   // integers with a negative minimum, stored + STORAGE_OFFSET (CT: HU + 1024)
    Linear,   // anything else, mapped linearly from its minimum-maximum to 0-65535
};

struct StorageChoice {
    StorageKind kind = StorageKind::Identity;
    int bit_depth = 16;
    // value = stored × scale + offset
    double scale = 1;
    double offset = 0;
};

// Added to integer values with a negative minimum; CT values (HU) then start at 0 for -1024 HU, below air
inline constexpr double STORAGE_OFFSET = 1024;

// Chooses the storage of values:
// - Identity for integers within 0-65535; 8 bits when eight_bit (8-bit samples) and the maximum is at most 255
// - Offset for integers with a negative minimum, when the minimum is at least -1024 or clip_below_offset is set (CT,
//   where lower values mark pixels outside the scan field) and the maximum still fits
// - Linear otherwise: the minimum is stored as 0 and the maximum as 65535
StorageChoice ChooseStorage(const ValueRange& range, bool eight_bit, bool clip_below_offset);

// The stored sample of a value: round((value - offset) / scale), clamped to the bit depth's range; non-finite values give 0
int StoredSample(double value, const StorageChoice& choice);

// The value in terms of the stored sample, e.g. "HU = stored value - 1024", "value = stored value × 0.02 - 3.5" or
// "value = 4095 - stored value"
std::string ConversionFormula(double scale, double offset, const std::string& unit);

// A number with up to 6 significant digits and no trailing zeros
std::string FormatValue(double value);

} // namespace glcm

#endif // GLCM_VALUE_CONVERSION_HPP_
