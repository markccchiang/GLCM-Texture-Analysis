#ifndef GLCM_TEXTURE_FEATURE_ANALYSIS_HPP_
#define GLCM_TEXTURE_FEATURE_ANALYSIS_HPP_

#include <array>
#include <iostream>
#include <limits>
#include <map>
#include <opencv2/opencv.hpp>
#include <set>
#include <string>
#include <vector>

namespace glcm {

enum class Type {
    Mean,
    Std,
    Energy,
    HomogeneityII,
    Contrast,
    SumOfSquares,
    CorrelationIII,
    Entropy,
    ClusterShade,
    ClusterProminence,
    AutoCorrelation,
    ContrastAnotherWay,
    CorrelationI,
    CorrelationII,
    CorrelationIAnotherWay,
    CorrelationIIAnotherWay,
    Dissimilarity,
    HomogeneityI,
    MaximumProbability,
    SumOfSquaresI,
    SumOfSquaresJ,
    SumAverage,
    SumEntropy,
    SumVariance,
    DifferenceVariance,
    DifferenceEntropy,
    InformationMeasuresOfCorrelationI,
    InformationMeasuresOfCorrelationII,
    InverseDifferenceNormalized,
    InverseDifferenceMomentNormalized,
    Score,
    Age
};

enum class Direction { H, V, LD, RD, Avg };

// One feature value per direction
struct Features {
    double H = 0.0;
    double V = 0.0;
    double LD = 0.0;
    double RD = 0.0;

    double Avg() const {
        return ((H + V + LD + RD) / 4.0);
    }

    double Get(Direction direction) const {
        switch (direction) {
            case Direction::H:
                return H;
            case Direction::V:
                return V;
            case Direction::LD:
                return LD;
            case Direction::RD:
                return RD;
            default:
                return Avg();
        }
    }
};

class TextureAnalysis {
public:
    explicit TextureAnalysis(int Ng);
    ~TextureAnalysis() = default;

    void ProcessRectImage(const cv::Mat& image, int distance);
    void ProcessPolygonImage(const cv::Mat& original_image, const cv::Mat& mask_image, int distance);

    void GetMean(Features& f) const;                                            // Mean of selected region pixels
    void GetStd(Features& f) const;                                             // STD of selected region pixels
    void GetAutoCorrelation(Features& f) const;                                 // F1: Auto Correlation
    void GetContrast(Features& f) const;                                        // F2: Contrast
    void GetContrastAnotherWay(Features& f) const;                              // F2: Contrast (another way)
    void GetCorrelationI(Features& f) const;                                    // F3: Correlation - I
    void GetCorrelationIAnotherWay(Features& f) const;                          // F3: Correlation - I (another way)
    void GetCorrelationII(Features& f) const;                                   // F4: Correlation - II
    void GetCorrelationIIAnotherWay(Features& f) const;                         // F4: Correlation - II (another way)
    void GetCorrelationIII(Features& f) const;                                  // F4: Correlation - III (Xiaofeng Yang's paper)
    void GetClusterProminence(Features& f) const;                               // F5: Cluster Prominence
    void GetClusterShade(Features& f) const;                                    // F6: Cluster Shade
    void GetDissimilarity(Features& f) const;                                   // F7: Dissimilarity
    void GetEnergy(Features& f) const;                                          // F8: Energy (Angular Second Moment)
    void GetEntropy(Features& f) const;                                         // F9: Entropy
    void GetHomogeneityI(Features& f) const;                                    // F10: Homogeneity - I
    void GetHomogeneityII(Features& f) const;                                   // F11: Homogeneity - II (Inverse Difference Moment)
    void GetMaximumProbability(Features& f) const;                              // F12: Maximum Probability
    void GetSumOfSquares(Features& f) const;                                    // F13: Sum of Squares (Variance in i and j)
    void GetSumOfSquares_i(Features& f) const;                                  // F13: Sum of Squares (Variance in i)
    void GetSumOfSquares_j(Features& f) const;                                  // F13: Sum of Squares (Variance in j)
    void GetSumAverage(Features& f) const;                                      // F14: Sum Average
    void GetSumEntropy(Features& f) const;                                      // F15: Sum Entropy
    void GetSumVariance(Features& f) const;                                     // F16: Sum Variance
    void GetDifferenceVariance(Features& f) const;                              // F17: Difference Variance
    void GetDifferenceEntropy(Features& f) const;                               // F18: Difference Entropy
    void GetInformationMeasuresOfCorrelation(Features& f1, Features& f2) const; // F19, F20: Information Measures of Correlation - I/II
    void GetInverseDifferenceNormalized(Features& f) const;                     // F21: Inverse Difference Normalized
    void GetInverseDifferenceMomentNormalized(Features& f) const;               // F22: Inverse Difference Moment Normalized
    void GetMaximalCorrelationCoefficient(Features& f) const;                   // Maximal Correlation Coefficient

    std::map<Type, Features> Calculate(const std::set<Type>& types) const; // Calculate selected features
    void CalculateScore(double age, std::map<Type, Features>& features_map) const;

    void Print(const std::map<Type, Features>& features) const;
    void SaveAsCSV(const std::string& image_name, const std::map<Type, Features>& features, const std::string& csv_name) const;

private:
    // Probabilities of one direction (H = 0, V = 90, LD = 135, RD = 45 degrees), filled by Process()
    struct DirectionData {
        std::vector<double> p;     // normalized GLCM p(i, j), stored row by row (Ng x Ng)
        std::vector<double> px;    // p_x(i) = sum_j p(i, j)
        std::vector<double> py;    // p_y(j) = sum_i p(i, j)
        std::vector<double> p_xpy; // p_{x+y}(k), k = i + j
        std::vector<double> p_xny; // p_{x-y}(k), k = |i - j|

        // means and STDs from the marginal vectors p_x and p_y
        double mu_x = 0.0;
        double mu_y = 0.0;
        double sigma_x = 0.0;
        double sigma_y = 0.0;

        // the same statistics summed directly over p(i, j), used by the "another way" cross-check features
        double glcm_mu_i = 0.0;
        double glcm_mu_j = 0.0;
        double glcm_sigma_i = 0.0;
        double glcm_sigma_j = 0.0;
    };

    void Process(const cv::Mat& image, const cv::Mat* mask, int distance);
    void Normalize(DirectionData& data, const std::vector<int>& counts, int total) const;
    void CalculatePixelStatistics(const std::vector<double>& pixel_values);

    // Evaluates fn(DirectionData) for H, V, LD and RD
    template <typename Fn>
    Features ForEachDirection(Fn fn) const;

    double P(const DirectionData& data, int i, int j) const {
        return data.p[i * _Ng + j];
    }

    static std::string TypeToString(Type type);
    static std::string DirectionToString(Direction direction);
    static std::string GetCurrentTime();

    int _Ng; // grey scale number, 256 (0 ~ 255) for example

    std::array<DirectionData, 4> _directions; // indexed in the order H, V, LD, RD

    double _pixel_values_mean = std::numeric_limits<double>::quiet_NaN();
    double _pixel_values_STD = std::numeric_limits<double>::quiet_NaN();
};

} // namespace glcm

#endif // GLCM_TEXTURE_FEATURE_ANALYSIS_HPP_
