#ifndef GLCM_SCORE_HPP_
#define GLCM_SCORE_HPP_

#include "analysis/TextureAnalysis.hpp"

namespace glcm {

// Coefficients of the age-based score:
//   Score = age_coefficient * age + mean * Mean + entropy * Entropy + contrast * Contrast
// The defaults are the values used by the original application, fitted with Ng = 256, d = 1, the mean of all four
// directions, rectangle/polygon ROIs on 8-bit images, and age in years.
struct ScoreCoefficients {
    double age = 1.138;
    double mean = -1.814;
    double entropy = 1.416;
    double contrast = 1.714;
};

// Score per direction; a direction where any input is NaN gives NaN
Features ComputeScore(double age, const Features& mean, const Features& entropy, const Features& contrast,
    const ScoreCoefficients& coefficients = ScoreCoefficients());

} // namespace glcm

#endif // GLCM_SCORE_HPP_
