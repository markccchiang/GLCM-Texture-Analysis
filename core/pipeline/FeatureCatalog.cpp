#include "pipeline/FeatureCatalog.hpp"

#include <stdexcept>

namespace glcm {

namespace {

const char YANG_CORRELATION[] =
    "Follows Yang et al. (2012) as printed: the covariance is divided by the product of the variances "
    "instead of the standard deviations, so the value is not bounded like a correlation. Correlation II "
    "is the standard definition.";

const char YANG_SUM_OF_SQUARES[] =
    "Follows Yang et al. (2012): the sum of the variances in i and j, which is twice Haralick's F4 for "
    "a symmetric co-occurrence matrix. Sum of Squares (in x) is Haralick's F4.";

const char REGION_ANCHOR[] = "equations.html#first-order-statistics";
const char HARALICK_ANCHOR[] = "equations.html#haralick-features";
const char OTHER_ANCHOR[] = "equations.html#other-co-occurrence-features";
const char RUN_LENGTH_ANCHOR[] = "equations.html#run-length-features-glrlm";

FeatureInfo Make(Type type, const char* id, FeatureGroup group, const char* anchor, FeatureCost cost = FeatureCost::Normal,
    const char* non_standard_reason = "") {
    return FeatureInfo{
        type, id, TextureAnalysis::TypeToString(type), group, non_standard_reason[0] != '\0', non_standard_reason, anchor, cost};
}

std::vector<FeatureInfo> BuildCatalog() {
    using G = FeatureGroup;
    return {
        Make(Type::Mean, "Mean", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Std, "Std", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Minimum, "Minimum", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Maximum, "Maximum", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Range, "Range", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Median, "Median", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Percentile10, "Percentile10", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Percentile90, "Percentile90", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::InterquartileRange, "InterquartileRange", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::MeanAbsoluteDeviation, "MeanAbsoluteDeviation", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::RobustMeanAbsoluteDeviation, "RobustMeanAbsoluteDeviation", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::RootMeanSquared, "RootMeanSquared", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::FirstOrderEnergy, "FirstOrderEnergy", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Variance, "Variance", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Skewness, "Skewness", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Kurtosis, "Kurtosis", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::FirstOrderEntropy, "FirstOrderEntropy", G::RegionStatistics, REGION_ANCHOR),
        Make(Type::Uniformity, "Uniformity", G::RegionStatistics, REGION_ANCHOR),

        Make(Type::Energy, "Energy", G::Haralick, HARALICK_ANCHOR),
        Make(Type::Contrast, "Contrast", G::Haralick, HARALICK_ANCHOR),
        Make(Type::ContrastAnotherWay, "ContrastAnotherWay", G::Haralick, HARALICK_ANCHOR),
        Make(Type::CorrelationII, "CorrelationII", G::Haralick, HARALICK_ANCHOR),
        Make(Type::CorrelationIIAnotherWay, "CorrelationIIAnotherWay", G::Haralick, HARALICK_ANCHOR),
        Make(Type::SumOfSquares, "SumOfSquares", G::Haralick, HARALICK_ANCHOR, FeatureCost::Normal, YANG_SUM_OF_SQUARES),
        Make(Type::SumOfSquaresI, "SumOfSquaresI", G::Haralick, HARALICK_ANCHOR),
        Make(Type::SumOfSquaresJ, "SumOfSquaresJ", G::Haralick, HARALICK_ANCHOR),
        Make(Type::HomogeneityII, "HomogeneityII", G::Haralick, HARALICK_ANCHOR),
        Make(Type::SumAverage, "SumAverage", G::Haralick, HARALICK_ANCHOR),
        Make(Type::SumVariance, "SumVariance", G::Haralick, HARALICK_ANCHOR),
        Make(Type::SumEntropy, "SumEntropy", G::Haralick, HARALICK_ANCHOR),
        Make(Type::Entropy, "Entropy", G::Haralick, HARALICK_ANCHOR),
        Make(Type::DifferenceVariance, "DifferenceVariance", G::Haralick, HARALICK_ANCHOR),
        Make(Type::DifferenceEntropy, "DifferenceEntropy", G::Haralick, HARALICK_ANCHOR),
        Make(Type::InformationMeasuresOfCorrelationI, "InformationMeasuresOfCorrelationI", G::Haralick, HARALICK_ANCHOR),
        Make(Type::InformationMeasuresOfCorrelationII, "InformationMeasuresOfCorrelationII", G::Haralick, HARALICK_ANCHOR),
        Make(Type::MaximalCorrelationCoefficient, "MaximalCorrelationCoefficient", G::Haralick, HARALICK_ANCHOR, FeatureCost::Slow),

        Make(Type::AutoCorrelation, "AutoCorrelation", G::Other, OTHER_ANCHOR),
        Make(Type::CorrelationI, "CorrelationI", G::Other, OTHER_ANCHOR),
        Make(Type::CorrelationIAnotherWay, "CorrelationIAnotherWay", G::Other, OTHER_ANCHOR),
        Make(Type::CorrelationIII, "CorrelationIII", G::Other, OTHER_ANCHOR, FeatureCost::Normal, YANG_CORRELATION),
        Make(Type::ClusterShade, "ClusterShade", G::Other, OTHER_ANCHOR),
        Make(Type::ClusterProminence, "ClusterProminence", G::Other, OTHER_ANCHOR),
        Make(Type::Dissimilarity, "Dissimilarity", G::Other, OTHER_ANCHOR),
        Make(Type::HomogeneityI, "HomogeneityI", G::Other, OTHER_ANCHOR),

        Make(Type::GlrlmShortRunEmphasis, "GlrlmShortRunEmphasis", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmLongRunEmphasis, "GlrlmLongRunEmphasis", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmGrayLevelNonUniformity, "GlrlmGrayLevelNonUniformity", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmGrayLevelNonUniformityNormalized, "GlrlmGrayLevelNonUniformityNormalized", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmRunLengthNonUniformity, "GlrlmRunLengthNonUniformity", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmRunLengthNonUniformityNormalized, "GlrlmRunLengthNonUniformityNormalized", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmRunPercentage, "GlrlmRunPercentage", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmGrayLevelVariance, "GlrlmGrayLevelVariance", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmRunVariance, "GlrlmRunVariance", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmRunEntropy, "GlrlmRunEntropy", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmLowGrayLevelRunEmphasis, "GlrlmLowGrayLevelRunEmphasis", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmHighGrayLevelRunEmphasis, "GlrlmHighGrayLevelRunEmphasis", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmShortRunLowGrayLevelEmphasis, "GlrlmShortRunLowGrayLevelEmphasis", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmShortRunHighGrayLevelEmphasis, "GlrlmShortRunHighGrayLevelEmphasis", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmLongRunLowGrayLevelEmphasis, "GlrlmLongRunLowGrayLevelEmphasis", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::GlrlmLongRunHighGrayLevelEmphasis, "GlrlmLongRunHighGrayLevelEmphasis", G::RunLength, RUN_LENGTH_ANCHOR),
        Make(Type::MaximumProbability, "MaximumProbability", G::Other, OTHER_ANCHOR),
        Make(Type::InverseDifferenceNormalized, "InverseDifferenceNormalized", G::Other, OTHER_ANCHOR),
        Make(Type::InverseDifferenceMomentNormalized, "InverseDifferenceMomentNormalized", G::Other, OTHER_ANCHOR),
    };
}

} // namespace

const std::vector<FeatureInfo>& FeatureCatalog() {
    static const std::vector<FeatureInfo> catalog = BuildCatalog();
    return catalog;
}

const FeatureInfo& FindFeature(Type type) {
    for (const FeatureInfo& info : FeatureCatalog()) {
        if (info.type == type) {
            return info;
        }
    }
    throw std::invalid_argument("Not a selectable feature: " + TextureAnalysis::TypeToString(type));
}

std::optional<Type> FeatureTypeFromId(const std::string& id) {
    for (const FeatureInfo& info : FeatureCatalog()) {
        if (info.id == id) {
            return info.type;
        }
    }
    return std::nullopt;
}

const std::vector<FeaturePreset>& FeaturePresets() {
    static const std::vector<FeaturePreset> presets = [] {
        std::set<Type> all;
        std::set<Type> first_order;
        std::set<Type> run_length;
        for (const FeatureInfo& info : FeatureCatalog()) {
            all.insert(info.type);
            if (info.group == FeatureGroup::RegionStatistics) {
                first_order.insert(info.type);
            } else if (info.group == FeatureGroup::RunLength) {
                run_length.insert(info.type);
            }
        }
        return std::vector<FeaturePreset>{
            {"haralick", "Haralick F1–F14",
                {Type::Energy, Type::Contrast, Type::CorrelationII, Type::SumOfSquaresI, Type::HomogeneityII, Type::SumAverage,
                    Type::SumVariance, Type::SumEntropy, Type::Entropy, Type::DifferenceVariance, Type::DifferenceEntropy,
                    Type::InformationMeasuresOfCorrelationI, Type::InformationMeasuresOfCorrelationII, Type::MaximalCorrelationCoefficient},
                false},
            {"clausi2002", "Clausi (2002): Contrast, Correlation, Entropy", {Type::Contrast, Type::CorrelationII, Type::Entropy}, false},
            {"basic", "Basic",
                {Type::Mean, Type::Std, Type::Contrast, Type::Entropy, Type::Energy, Type::HomogeneityII, Type::CorrelationII}, false},
            {"score", "Score (Mean, Entropy, Contrast)", {Type::Mean, Type::Entropy, Type::Contrast}, true},
            {"firstOrder", "First-order statistics", first_order, false},
            {"glrlm", "Run length (GLRLM)", run_length, false},
            {"all", "All features", all, false},
        };
    }();
    return presets;
}

std::string FeatureGroupId(FeatureGroup group) {
    switch (group) {
        case FeatureGroup::RegionStatistics:
            return "regionStatistics";
        case FeatureGroup::Haralick:
            return "haralick";
        case FeatureGroup::Other:
            return "other";
        case FeatureGroup::RunLength:
            return "runLength";
    }
    throw std::invalid_argument("Unknown feature group");
}

} // namespace glcm
