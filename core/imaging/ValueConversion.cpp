#include "imaging/ValueConversion.hpp"

#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>

namespace glcm {

namespace {

constexpr double MAX_16_BIT = 65535;
constexpr double MAX_8_BIT = 255;

} // namespace

StorageChoice ChooseStorage(const ValueRange& range, bool eight_bit, bool clip_below_offset) {
    StorageChoice choice;
    if (range.integral && range.minimum >= 0 && range.maximum <= MAX_16_BIT) {
        choice.kind = StorageKind::Identity;
        choice.bit_depth = (eight_bit && range.maximum <= MAX_8_BIT) ? 8 : 16;
        return choice;
    }
    if (range.integral && range.minimum < 0 && (range.minimum >= -STORAGE_OFFSET || clip_below_offset) &&
        range.maximum + STORAGE_OFFSET <= MAX_16_BIT) {
        choice.kind = StorageKind::Offset;
        choice.offset = -STORAGE_OFFSET;
        return choice;
    }
    choice.kind = StorageKind::Linear;
    choice.offset = range.minimum;
    choice.scale = range.maximum > range.minimum ? (range.maximum - range.minimum) / MAX_16_BIT : 1;
    return choice;
}

int StoredSample(double value, const StorageChoice& choice) {
    const double stored = std::round((value - choice.offset) / choice.scale);
    if (!std::isfinite(stored)) {
        return 0;
    }
    return static_cast<int>(std::clamp(stored, 0.0, choice.bit_depth == 8 ? MAX_8_BIT : MAX_16_BIT));
}

std::string FormatValue(double value) {
    if (value == 0) {
        return "0"; // no "-0"
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out.precision(6);
    out << value;
    return out.str();
}

std::string ConversionFormula(double scale, double offset, const std::string& unit) {
    std::string formula = (unit.empty() ? "value" : unit) + " = ";
    if (scale == -1) {
        formula += FormatValue(offset) + " - stored value";
        return formula;
    }
    formula += scale == 1 ? "stored value" : "stored value × " + FormatValue(scale);
    if (offset > 0) {
        formula += " + " + FormatValue(offset);
    } else if (offset < 0) {
        formula += " - " + FormatValue(-offset);
    }
    return formula;
}

} // namespace glcm
