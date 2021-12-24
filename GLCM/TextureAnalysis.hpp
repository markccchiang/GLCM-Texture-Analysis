#ifndef GLCM_TEXTURE_FEATURE_ANALYSIS_HPP_
#define GLCM_TEXTURE_FEATURE_ANALYSIS_HPP_

#include <iostream>
#include <map>
#include <opencv2/opencv.hpp>
#include <set>
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

struct Features {
    double H;
    double V;
    double LD;
    double RD;

    Features operator()(double H_, double V_, double LD_, double RD_) {
        H = H_;
        V = V_;
        LD = LD_;
        RD = RD_;
        return Features();
    }

    double Avg() {
        return ((H + V + LD + RD) / 4.0);
    }
};

class TextureAnalysis {
public:
    TextureAnalysis(int Ng);
    ~TextureAnalysis() = default;

    void ProcessRectImage(const cv::Mat& image, int distance);
    void ProcessPolygonImage(const cv::Mat& original_image, const cv::Mat& mask_image, int distance);

    void GetMean(Features& f);                                            // Mean of selected region pixels
    void GetStd(Features& f);                                             // STD of selected region pixels
    void GetAutoCorrelation(Features& f);                                 // F1: Auto Correlation
    void GetContrast(Features& f);                                        // F2: Contrast
    void GetContrastAnotherWay(Features& f);                              // F2: Contrast (another way)
    void GetCorrelationI(Features& f);                                    // F3: Correlation - I
    void GetCorrelationIAnotherWay(Features& f);                          // F3: Correlation - I (another way)
    void GetCorrelationII(Features& f);                                   // F4: Correlation - II
    void GetCorrelationIIAnotherWay(Features& f);                         // F4: Correlation - II (another way)
    void GetCorrelationIII(Features& f);                                  // F4: Correlation - III (Xiaofeng Yang's paper)
    void GetClusterProminence(Features& f);                               // F5: Cluster Prominence
    void GetClusterShade(Features& f);                                    // F6: Cluster Shade
    void GetDissimilarity(Features& f);                                   // F7: Dissimilarity
    void GetEnergy(Features& f);                                          // F8: Energy (Angular Second Moment)
    void GetEntropy(Features& f);                                         // F9: Entropy
    void GetHomogeneityI(Features& f);                                    // F10: Homogeneity - I
    void GetHomogeneityII(Features& f);                                   // F11: Homogeneity - II (Inverse Difference Moment)
    void GetMaximumProbability(Features& f);                              // F12: Maximum Probability
    void GetSumOfSquares(Features& f);                                    // F13: Sum of Squares (Variance in i and j)
    void GetSumOfSquares_i(Features& f);                                  // F13: Sum of Squares (Variance in i)
    void GetSumOfSquares_j(Features& f);                                  // F13: Sum of Squares (Variance in j)
    void GetSumAverage(Features& f);                                      // F14: Sum Average
    void GetSumEntropy(Features& f);                                      // F15: Sum Entropy
    void GetSumVariance(Features& f);                                     // F16: Sum Variance
    void GetDifferenceVariance(Features& f);                              // F17: Difference Variance
    void GetDifferenceEntropy(Features& f);                               // F18: Difference Entropy
    void GetInformationMeasuresOfCorrelation(Features& f1, Features& f2); // F19, F20: Information Measures of Correlation - I/II
    void GetInverseDifferenceNormalized(Features& f);                     // F21: Inverse Difference Normalized
    void GetInverseDifferenceMomentNormalized(Features& f);               // F22: Inverse Difference Moment Normalized
    void GetMaximalCorrelationCoefficient(Features& f);                   // Maximal Correlation Coefficient

    std::map<Type, Features> Calculate(const std::set<Type>& types); // Calculate selected features
    void CalculateScore(double age, std::map<Type, Features>& features_map);

    void Print(const std::map<Type, Features>& features);
    void SaveAsCSV(const std::string& image_name, std::map<Type, Features> features, const std::string& csv_name);

private:
    void ResetCache();

    void CountElemH(int i, int j);
    void CountElemV(int i, int j);
    void CountElemLD(int i, int j);
    void CountElemRD(int i, int j);
    void PushPixelValue(int pixel_value);

    void Normalization();

    void Calculate_px();
    void Calculate_py();
    void Calculate_p_xpy();
    void Calculate_p_xny();

    double CalculateMean(const std::vector<double>& vec);
    double CalculateSTD(const std::vector<double>& vec);
    double CalculateGLCMMean_i(const std::vector<std::vector<double>>& mat);
    double CalculateGLCMMean_j(const std::vector<std::vector<double>>& mat);
    double CalculateGLCMSTD_i(const std::vector<std::vector<double>>& mat);
    double CalculateGLCMSTD_j(const std::vector<std::vector<double>>& mat);
    Features CalculateQ(int i, int j);

    void CalculateHX();
    void CalculateHY();
    void CalculateHXY();
    void CalculateHXY1();
    void CalculateHXY2();

    void ResetFactors();

    std::string TypeToString(const Type& type);
    std::string DirectionToString(const Direction& direction);
    std::string GetCurrentTime();

    void CalculatePixelMean(const std::vector<double>& vec);
    void CalculatePixelSTD(const std::vector<double>& vec);

    int _Ng; // grey scale number, 256 (0 ~ 255) for example

    int _R_H;  // normalization factor for 0 degree matrix
    int _R_V;  // normalization factor for 90 degree matrix
    int _R_LD; // normalization factor for 135 degree matrix
    int _R_RD; // normalization factor for 45 degree matrix

    std::vector<std::vector<int>> _P_H;  // 0 degree matrix
    std::vector<std::vector<int>> _P_V;  // 90 degree matrix
    std::vector<std::vector<int>> _P_LD; // 135 degree matrix
    std::vector<std::vector<int>> _P_RD; // 45 degree matrix

    std::vector<std::vector<double>> _p_H;  // 0 degree matrix
    std::vector<std::vector<double>> _p_V;  // 90 degree matrix
    std::vector<std::vector<double>> _p_LD; // 135 degree matrix
    std::vector<std::vector<double>> _p_RD; // 45 degree matrix

    std::vector<double> _pixel_values; // pixel values in the region
    double _pixel_values_mean;
    double _pixel_values_STD;

    // "p_x"
    std::vector<double> _px_H;
    std::vector<double> _px_V;
    std::vector<double> _px_LD;
    std::vector<double> _px_RD;

    // "p_y"
    std::vector<double> _py_H;
    std::vector<double> _py_V;
    std::vector<double> _py_LD;
    std::vector<double> _py_RD;

    // "p_{x+y}"
    std::vector<double> _p_xpy_H;
    std::vector<double> _p_xpy_V;
    std::vector<double> _p_xpy_LD;
    std::vector<double> _p_xpy_RD;

    // "p_{x-y}"
    std::vector<double> _p_xny_H;
    std::vector<double> _p_xny_V;
    std::vector<double> _p_xny_LD;
    std::vector<double> _p_xny_RD;

    double _HX_H;
    double _HX_V;
    double _HX_LD;
    double _HX_RD;

    double _HY_H;
    double _HY_V;
    double _HY_LD;
    double _HY_RD;

    double _HXY_H;
    double _HXY_V;
    double _HXY_LD;
    double _HXY_RD;

    double _HXY1_H;
    double _HXY1_V;
    double _HXY1_LD;
    double _HXY1_RD;

    double _HXY2_H;
    double _HXY2_V;
    double _HXY2_LD;
    double _HXY2_RD;
};

} // namespace glcm

#endif // GLCM_TEXTURE_FEATURE_ANALYSIS_HPP_
