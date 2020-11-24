#ifndef GLCM_TEXTURE_FEATURE_ANALYSIS_HPP_
#define GLCM_TEXTURE_FEATURE_ANALYSIS_HPP_

#include <iostream>
#include <vector>

namespace glcm {

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
    ~TextureAnalysis(){};

    void ResetCache();

    void CountElemH(int i, int j);
    void CountElemV(int i, int j);
    void CountElemLD(int i, int j);
    void CountElemRD(int i, int j);

    void Normalization();

    // Calculate f1: Angular Second Moment
    void GetAngularSecondMoment(Features& f);
    // Calculate f2: Contrast
    void GetContrast(Features& f);
    // Calculate f3: Correlation
    void GetCorrelation(Features& f);
    // Calculate f4: Sum of Squares: Variance
    void GetVariance(Features& f);
    // Calculate f5: Inverse Difference Moment
    void GetInverseDifferenceMoment(Features& f);
    // Calculate f6: Sum Average
    void GetSumAverage(Features& f);
    // Calculate f7: Sum Variance
    void GetSumVariance(Features& f);
    // Calculate f8: Sum Entropy
    void GetSumEntropy(Features& f);
    // Calculate f9: Entropy
    void GetEntropy(Features& f);
    // Calculate f10: Difference Variance
    void GetDifferenceVariance(Features& f);
    // Calculate f11: Difference Entropy
    void GetDifferenceEntropy(Features& f);
    // Calculate f12, f13: Information Measures of Correlation
    void GetInformationMeasuresOfCorrelation(Features& f1, Features& f2);
    // Calculate f14: Maximal Correlation Coefficient
    void GetMaximalCorrelationCoefficient(Features& f);

private:
    void Calculate_px();
    void Calculate_py();
    void Calculate_p_xpy();
    void Calculate_p_xny();

    double CalculateMean(const std::vector<double>& vec);
    double CalculateSTD(const std::vector<double>& vec);
    double CalculateGLCMMean(const std::vector<std::vector<double>>& mat);
    Features CalculateQ(int i, int j);

    void CalculateHX();
    void CalculateHY();
    void CalculateHXY();
    void CalculateHXY1();
    void CalculateHXY2();

    void ResetFactors();

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
