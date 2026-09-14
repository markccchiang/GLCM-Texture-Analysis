#include "analysis/Score.hpp"

namespace glcm {

Features ComputeScore(
    double age, const Features& mean, const Features& entropy, const Features& contrast, const ScoreCoefficients& coefficients) {
    auto score = [&](Direction direction) {
        return coefficients.age * age + coefficients.mean * mean.Get(direction) + coefficients.entropy * entropy.Get(direction) +
               coefficients.contrast * contrast.Get(direction);
    };
    return {score(Direction::H), score(Direction::V), score(Direction::LD), score(Direction::RD)};
}

} // namespace glcm
