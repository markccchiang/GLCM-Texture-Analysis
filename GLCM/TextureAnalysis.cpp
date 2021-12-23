#include "TextureAnalysis.hpp"

#include <math.h>

#include <Eigen/Eigenvalues>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>

const int white_color = 255;
const int black_color = 0;

using namespace glcm;

namespace fs = std::filesystem;

TextureAnalysis::TextureAnalysis(int Ng) : _Ng(Ng) {
    if (Ng > 0) {
        // initialize probability matrices
        _P_H.resize(_Ng, std::vector<int>(_Ng));
        _P_V.resize(_Ng, std::vector<int>(_Ng));
        _P_LD.resize(_Ng, std::vector<int>(_Ng));
        _P_RD.resize(_Ng, std::vector<int>(_Ng));

        _p_H.resize(_Ng, std::vector<double>(_Ng));
        _p_V.resize(_Ng, std::vector<double>(_Ng));
        _p_LD.resize(_Ng, std::vector<double>(_Ng));
        _p_RD.resize(_Ng, std::vector<double>(_Ng));

        // initialize probability vectors
        _px_H.resize(_Ng);
        _px_V.resize(_Ng);
        _px_LD.resize(_Ng);
        _px_RD.resize(_Ng);

        _py_H.resize(_Ng);
        _py_V.resize(_Ng);
        _py_LD.resize(_Ng);
        _py_RD.resize(_Ng);

        _p_xpy_H.resize(2 * _Ng - 1);
        _p_xpy_V.resize(2 * _Ng - 1);
        _p_xpy_LD.resize(2 * _Ng - 1);
        _p_xpy_RD.resize(2 * _Ng - 1);

        _p_xny_H.resize(_Ng);
        _p_xny_V.resize(_Ng);
        _p_xny_LD.resize(_Ng);
        _p_xny_RD.resize(_Ng);

        // reset factors as zeros
        ResetFactors();
    } else {
        std::cerr << "Invalid Ng assignment (Ng < 0)!\n";
    }
}

void TextureAnalysis::ProcessRectImage(const cv::Mat& image, int distance) {
    // Clear the cache
    ResetCache();

    // Calculate matrices elements: central pixel coord (m ,n), where "m" is the row index, and "n" is the column index
    for (int m = 0; m < image.rows; ++m) {
        for (int n = 0; n < image.cols; ++n) {
            // Nearest neighborhood pixel coord (k ,l), where "k" is the row index, and "l" is the column index
            for (int k = m - distance; k <= m + distance; ++k) {
                for (int l = n - distance; l <= n + distance; ++l) {
                    if ((k >= 0) && (l >= 0) && (k < image.rows) && (l < image.cols)) {
                        //// ToDo: need to check are pixel values in coordinates (m,n) and (k,l) nan or masked!!
                        int j = (int)(image.at<uchar>(m, n)); // I(m,n)
                        int i = (int)(image.at<uchar>(k, l)); // I(k,l)
                        if (((k - m) == 0) && (abs(l - n) == distance)) {
                            CountElemH(i, j);
                        } else if ((((k - m) == distance) && ((l - n) == -distance)) || (((k - m) == -distance) && ((l - n) == distance))) {
                            CountElemRD(i, j);
                        } else if ((abs(k - m) == distance) && (l - n == 0)) {
                            CountElemV(i, j);
                        } else if ((((k - m) == distance) && ((l - n) == distance)) || (((k - m) == -distance) && ((l - n) == -distance))) {
                            CountElemLD(i, j);
                        } else if ((m == k) && (n == l)) {
                            PushPixelValue(i);
                        } else {
                            // if ((m != k) || (n != l)) {
                            //    cerr << "unknown element:" << endl;
                            //    cerr << "central element: (m, n) = (" << m << "," << n << ")" << endl;
                            //    cerr << "neighborhood element: (k, l) = (" << k << "," << l << ")" << endl;
                            //}
                        }
                    }
                }
            }
        }
    }

    // Normalize the matrices
    Normalization();
}

void TextureAnalysis::ProcessPolygonImage(const cv::Mat& original_image, const cv::Mat& mask_image, int distance) {
    // Clear the cache
    ResetCache();

    // Calculate matrices elements: central pixel coord (m ,n), where "m" is the row index, and "n" is the column index
    int masked = 0;
    int non_masked = 0;
    for (int m = 0; m < original_image.rows; ++m) {
        for (int n = 0; n < original_image.cols; ++n) {
            // Nearest neighborhood pixel coord (k ,l), where "k" is the row index, and "l" is the column index
            for (int k = m - distance; k <= m + distance; ++k) {
                for (int l = n - distance; l <= n + distance; ++l) {
                    if ((k >= 0) && (l >= 0) && (k < original_image.rows) && (l < original_image.cols)) {
                        // Check is the nearest neighborhood pixel coord (k ,l) masked
                        int mask_pixel_value = (int)(mask_image.at<uchar>(k, l));
                        if (mask_pixel_value == white_color) {             // if nearest neighborhood pixel coord (k ,l) is not masked
                            int j = (int)(original_image.at<uchar>(m, n)); // I(m,n)
                            int i = (int)(original_image.at<uchar>(k, l)); // I(k,l)
                            if (((k - m) == 0) && (abs(l - n) == distance)) {
                                CountElemH(i, j);
                            } else if ((((k - m) == distance) && ((l - n) == -distance)) ||
                                       (((k - m) == -distance) && ((l - n) == distance))) {
                                CountElemRD(i, j);
                            } else if ((abs(k - m) == distance) && (l - n == 0)) {
                                CountElemV(i, j);
                            } else if ((((k - m) == distance) && ((l - n) == distance)) ||
                                       (((k - m) == -distance) && ((l - n) == -distance))) {
                                CountElemLD(i, j);
                            } else if ((m == k) && (n == l)) {
                                PushPixelValue(i);
                            } else {
                                // if ((m != k) || (n != l)) {
                                //    cerr << "unknown element:" << endl;
                                //    cerr << "central element: (m, n) = (" << m << "," << n << ")" << endl;
                                //    cerr << "neighborhood element: (k, l) = (" << k << "," << l << ")" << endl;
                                //}
                            }
                            ++non_masked;
                        } else {
                            // cerr << "masked coord: (k, l) = (" << k << "," << l << "), pixel value = " << mask_pixel_value << endl;
                            ++masked;
                        }
                    }
                }
            }
        }
    }

    // Normalize the matrices
    Normalization();
}

void TextureAnalysis::ResetCache() {
    // reset probability matrices as zeros
    for (int i = 0; i < _Ng; ++i) {
        std::fill(_P_H[i].begin(), _P_H[i].end(), 0);
        std::fill(_P_V[i].begin(), _P_V[i].end(), 0);
        std::fill(_P_LD[i].begin(), _P_LD[i].end(), 0);
        std::fill(_P_RD[i].begin(), _P_RD[i].end(), 0);

        std::fill(_p_H[i].begin(), _p_H[i].end(), 0);
        std::fill(_p_V[i].begin(), _p_V[i].end(), 0);
        std::fill(_p_LD[i].begin(), _p_LD[i].end(), 0);
        std::fill(_p_RD[i].begin(), _p_RD[i].end(), 0);
    }

    _pixel_values.clear();
    _pixel_values_mean = std::numeric_limits<double>::quiet_NaN();
    _pixel_values_STD = std::numeric_limits<double>::quiet_NaN();

    // reset probability vectors as zeros
    std::fill(_px_H.begin(), _px_H.end(), 0);
    std::fill(_px_V.begin(), _px_V.end(), 0);
    std::fill(_px_LD.begin(), _px_LD.end(), 0);
    std::fill(_px_RD.begin(), _px_RD.end(), 0);

    std::fill(_py_H.begin(), _py_H.end(), 0);
    std::fill(_py_V.begin(), _py_V.end(), 0);
    std::fill(_py_LD.begin(), _py_LD.end(), 0);
    std::fill(_py_RD.begin(), _py_RD.end(), 0);

    std::fill(_p_xpy_H.begin(), _p_xpy_H.end(), 0);
    std::fill(_p_xpy_V.begin(), _p_xpy_V.end(), 0);
    std::fill(_p_xpy_LD.begin(), _p_xpy_LD.end(), 0);
    std::fill(_p_xpy_RD.begin(), _p_xpy_RD.end(), 0);

    std::fill(_p_xny_H.begin(), _p_xny_H.end(), 0);
    std::fill(_p_xny_V.begin(), _p_xny_V.end(), 0);
    std::fill(_p_xny_LD.begin(), _p_xny_LD.end(), 0);
    std::fill(_p_xny_RD.begin(), _p_xny_RD.end(), 0);

    // reset factors as zeros
    ResetFactors();
}

void TextureAnalysis::ResetFactors() {
    // reset normalization factors as zeros
    _R_H = 0;
    _R_V = 0;
    _R_LD = 0;
    _R_RD = 0;

    // initialize entropy factors
    _HX_H = 0;
    _HX_V = 0;
    _HX_LD = 0;
    _HX_RD = 0;

    _HY_H = 0;
    _HY_V = 0;
    _HY_LD = 0;
    _HY_RD = 0;

    _HXY_H = 0;
    _HXY_V = 0;
    _HXY_LD = 0;
    _HXY_RD = 0;

    _HXY1_H = 0;
    _HXY1_V = 0;
    _HXY1_LD = 0;
    _HXY1_RD = 0;

    _HXY2_H = 0;
    _HXY2_V = 0;
    _HXY2_LD = 0;
    _HXY2_RD = 0;
}

void TextureAnalysis::CountElemH(int i, int j) {
    ++_P_H[i][j];
    ++_R_H;
}

void TextureAnalysis::CountElemV(int i, int j) {
    ++_P_V[i][j];
    ++_R_V;
}

void TextureAnalysis::CountElemLD(int i, int j) {
    ++_P_LD[i][j];
    ++_R_LD;
}

void TextureAnalysis::CountElemRD(int i, int j) {
    ++_P_RD[i][j];
    ++_R_RD;
}

void TextureAnalysis::PushPixelValue(int pixel_value) {
    _pixel_values.push_back((double)pixel_value);
}

void TextureAnalysis::Normalization() {
    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            _p_H[i][j] = (double)_P_H[i][j] / (double)_R_H;
            _p_V[i][j] = (double)_P_V[i][j] / (double)_R_V;
            _p_LD[i][j] = (double)_P_LD[i][j] / (double)_R_LD;
            _p_RD[i][j] = (double)_P_RD[i][j] / (double)_R_RD;
        }
    }

    // calculate probability vectors
    Calculate_px();
    Calculate_py();
    Calculate_p_xpy();
    Calculate_p_xny();

    // calculate pixels mean and STD in the region
    CalculatePixelSTD(_pixel_values);
}

void TextureAnalysis::Calculate_px() {
    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            _px_H[i] += _p_H[i][j];
            _px_V[i] += _p_V[i][j];
            _px_LD[i] += _p_LD[i][j];
            _px_RD[i] += _p_RD[i][j];
        }
    }
}

void TextureAnalysis::Calculate_py() {
    for (int j = 0; j < _Ng; ++j) {
        for (int i = 0; i < _Ng; ++i) {
            _py_H[j] += _p_H[i][j];
            _py_V[j] += _p_V[i][j];
            _py_LD[j] += _p_LD[i][j];
            _py_RD[j] += _p_RD[i][j];
        }
    }
}

void TextureAnalysis::Calculate_p_xpy() {
    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            int k = i + j;
            _p_xpy_H[k] += _p_H[i][j];
            _p_xpy_V[k] += _p_V[i][j];
            _p_xpy_LD[k] += _p_LD[i][j];
            _p_xpy_RD[k] += _p_RD[i][j];
        }
    }
}

void TextureAnalysis::Calculate_p_xny() {
    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            int k = abs(i - j);
            _p_xny_H[k] += _p_H[i][j];
            _p_xny_V[k] += _p_V[i][j];
            _p_xny_LD[k] += _p_LD[i][j];
            _p_xny_RD[k] += _p_RD[i][j];
        }
    }
}

double TextureAnalysis::CalculateMean(const std::vector<double>& vec) {
    double sum = 0.0;
    for (int i = 0; i < vec.size(); ++i) {
        sum += i * vec[i];
    }
    return sum;
}

double TextureAnalysis::CalculateSTD(const std::vector<double>& vec) {
    double mean = CalculateMean(vec);
    double sum = 0.0;
    for (int i = 0; i < vec.size(); ++i) {
        sum += (i - mean) * (i - mean) * vec[i];
    }
    return sqrt(sum);
}

double TextureAnalysis::CalculateGLCMMean_i(const std::vector<std::vector<double>>& mat) {
    double mean = 0.0;
    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            mean += i * mat[i][j];
        }
    }
    return mean;
}

double TextureAnalysis::CalculateGLCMMean_j(const std::vector<std::vector<double>>& mat) {
    double mean = 0.0;
    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            mean += j * mat[i][j];
        }
    }
    return mean;
}

double TextureAnalysis::CalculateGLCMSTD_i(const std::vector<std::vector<double>>& mat) {
    double mu_x = CalculateGLCMMean_i(mat);
    double sigma_x = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            sigma_x += (i - mu_x) * (i - mu_x) * mat[i][j];
        }
    }

    return sqrt(sigma_x);
}

double TextureAnalysis::CalculateGLCMSTD_j(const std::vector<std::vector<double>>& mat) {
    double mu_y = CalculateGLCMMean_j(mat);
    double sigma_y = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            sigma_y += (j - mu_y) * (j - mu_y) * mat[i][j];
        }
    }

    return sqrt(sigma_y);
}

void TextureAnalysis::CalculateHX() {
    for (int i = 0; i < _Ng; ++i) {
        if (_px_H[i] > 0) {
            _HX_H -= _px_H[i] * log(_px_H[i]);
        }
        if (_px_V[i] > 0) {
            _HX_V -= _px_V[i] * log(_px_V[i]);
        }
        if (_px_LD[i] > 0) {
            _HX_LD -= _px_LD[i] * log(_px_LD[i]);
        }
        if (_px_RD[i] > 0) {
            _HX_RD -= _px_RD[i] * log(_px_RD[i]);
        }
    }
}

void TextureAnalysis::CalculateHY() {
    for (int i = 0; i < _Ng; ++i) {
        if (_py_H[i] > 0) {
            _HY_H -= _py_H[i] * log(_py_H[i]);
        }
        if (_py_V[i] > 0) {
            _HY_V -= _py_V[i] * log(_py_V[i]);
        }
        if (_py_LD[i] > 0) {
            _HY_LD -= _py_LD[i] * log(_py_LD[i]);
        }
        if (_py_RD[i] > 0) {
            _HY_RD -= _py_RD[i] * log(_py_RD[i]);
        }
    }
}

void TextureAnalysis::CalculateHXY() {
    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            if (_p_H[i][j] > 0) {
                _HXY_H -= _p_H[i][j] * log(_p_H[i][j]);
            }
            if (_p_V[i][j] > 0) {
                _HXY_V -= _p_V[i][j] * log(_p_V[i][j]);
            }
            if (_p_LD[i][j] > 0) {
                _HXY_LD -= _p_LD[i][j] * log(_p_LD[i][j]);
            }
            if (_p_RD[i][j] > 0) {
                _HXY_RD -= _p_RD[i][j] * log(_p_RD[i][j]);
            }
        }
    }
}

void TextureAnalysis::CalculateHXY1() {
    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            if (_px_H[i] * _py_H[j] > 0) {
                _HXY1_H -= _p_H[i][j] * log(_px_H[i] * _py_H[j]);
            }
            if (_px_V[i] * _py_V[j] > 0) {
                _HXY1_V -= _p_V[i][j] * log(_px_V[i] * _py_V[j]);
            }
            if (_px_LD[i] * _py_LD[j] > 0) {
                _HXY1_LD -= _p_LD[i][j] * log(_px_LD[i] * _py_LD[j]);
            }
            if (_px_RD[i] * _py_RD[j] > 0) {
                _HXY1_RD -= _p_RD[i][j] * log(_px_RD[i] * _py_RD[j]);
            }
        }
    }
}

void TextureAnalysis::CalculateHXY2() {
    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            if (_px_H[i] * _py_H[j] > 0) {
                _HXY2_H -= _px_H[i] * _py_H[j] * log(_px_H[i] * _py_H[j]);
            }
            if (_px_V[i] * _py_V[j] > 0) {
                _HXY2_V -= _px_V[i] * _py_V[j] * log(_px_V[i] * _py_V[j]);
            }
            if (_px_LD[i] * _py_LD[j] > 0) {
                _HXY2_LD -= _px_LD[i] * _py_LD[j] * log(_px_LD[i] * _py_LD[j]);
            }
            if (_px_RD[i] * _py_RD[j] > 0) {
                _HXY2_RD -= _px_RD[i] * _py_RD[j] * log(_px_RD[i] * _py_RD[j]);
            }
        }
    }
}

Features TextureAnalysis::CalculateQ(int i, int j) {
    double Q_H = 0.0;
    double Q_V = 0.0;
    double Q_LD = 0.0;
    double Q_RD = 0.0;

    for (int k = 0; k < _Ng; ++k) {
        if ((_px_H[i] * _py_H[k]) != 0) {
            Q_H += (_p_H[i][k] * _p_H[j][k]) / (_px_H[i] * _py_H[k]);
        }
        if ((_px_V[i] * _py_V[k]) != 0) {
            Q_V += (_p_V[i][k] * _p_V[j][k]) / (_px_V[i] * _py_V[k]);
        }
        if ((_px_LD[i] * _py_LD[k]) != 0) {
            Q_LD += (_p_LD[i][k] * _p_LD[j][k]) / (_px_LD[i] * _py_LD[k]);
        }
        if ((_px_RD[i] * _py_RD[k]) != 0) {
            Q_RD += (_p_RD[i][k] * _p_RD[j][k]) / (_px_RD[i] * _py_RD[k]);
        }
    }

    return {Q_H, Q_V, Q_LD, Q_RD};
}

//===============================================================================================================
// Calculate texture feature coefficients
//===============================================================================================================

void TextureAnalysis::GetEnergy(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += _p_H[i][j] * _p_H[i][j];
            f_V += _p_V[i][j] * _p_V[i][j];
            f_LD += _p_LD[i][j] * _p_LD[i][j];
            f_RD += _p_RD[i][j] * _p_RD[i][j];
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetContrast(Features& f) {
    std::vector<double> sub_H(_Ng);
    std::vector<double> sub_V(_Ng);
    std::vector<double> sub_LD(_Ng);
    std::vector<double> sub_RD(_Ng);

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            int n = abs(i - j);
            sub_H[n] += _p_H[i][j];
            sub_V[n] += _p_V[i][j];
            sub_LD[n] += _p_LD[i][j];
            sub_RD[n] += _p_RD[i][j];
        }
    }

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int n = 0; n < _Ng; ++n) {
        f_H += (n * n) * sub_H[n];
        f_V += (n * n) * sub_V[n];
        f_LD += (n * n) * sub_LD[n];
        f_RD += (n * n) * sub_RD[n];
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetContrastAnotherWay(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += (i - j) * (i - j) * _p_H[i][j];
            f_V += (i - j) * (i - j) * _p_V[i][j];
            f_LD += (i - j) * (i - j) * _p_LD[i][j];
            f_RD += (i - j) * (i - j) * _p_RD[i][j];
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetCorrelationI(Features& f) {
    // Calculate means
    double mu_x_H = CalculateMean(_px_H);
    double mu_x_V = CalculateMean(_px_V);
    double mu_x_LD = CalculateMean(_px_LD);
    double mu_x_RD = CalculateMean(_px_RD);

    double mu_y_H = CalculateMean(_py_H);
    double mu_y_V = CalculateMean(_py_V);
    double mu_y_LD = CalculateMean(_py_LD);
    double mu_y_RD = CalculateMean(_py_RD);

    // Calculate STDs
    double sigma_x_H = CalculateSTD(_px_H);
    double sigma_x_V = CalculateSTD(_px_V);
    double sigma_x_LD = CalculateSTD(_px_LD);
    double sigma_x_RD = CalculateSTD(_px_RD);

    double sigma_y_H = CalculateSTD(_py_H);
    double sigma_y_V = CalculateSTD(_py_V);
    double sigma_y_LD = CalculateSTD(_py_LD);
    double sigma_y_RD = CalculateSTD(_py_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += (i - mu_x_H) * (j - mu_y_H) * _p_H[i][j] / (sigma_x_H * sigma_y_H);
            f_V += (i - mu_x_V) * (j - mu_y_V) * _p_V[i][j] / (sigma_x_V * sigma_y_V);
            f_LD += (i - mu_x_LD) * (j - mu_y_LD) * _p_LD[i][j] / (sigma_x_LD * sigma_y_LD);
            f_RD += (i - mu_x_RD) * (j - mu_y_RD) * _p_RD[i][j] / (sigma_x_RD * sigma_y_RD);
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetCorrelationIAnotherWay(Features& f) {
    // Calculate means
    double mu_x_H = CalculateGLCMMean_i(_p_H);
    double mu_x_V = CalculateGLCMMean_i(_p_V);
    double mu_x_LD = CalculateGLCMMean_i(_p_LD);
    double mu_x_RD = CalculateGLCMMean_i(_p_RD);

    double mu_y_H = CalculateGLCMMean_j(_p_H);
    double mu_y_V = CalculateGLCMMean_j(_p_V);
    double mu_y_LD = CalculateGLCMMean_j(_p_LD);
    double mu_y_RD = CalculateGLCMMean_j(_p_RD);

    // Calculate STDs
    double sigma_x_H = CalculateGLCMSTD_i(_p_H);
    double sigma_x_V = CalculateGLCMSTD_i(_p_V);
    double sigma_x_LD = CalculateGLCMSTD_i(_p_LD);
    double sigma_x_RD = CalculateGLCMSTD_i(_p_RD);

    double sigma_y_H = CalculateGLCMSTD_j(_p_H);
    double sigma_y_V = CalculateGLCMSTD_j(_p_V);
    double sigma_y_LD = CalculateGLCMSTD_j(_p_LD);
    double sigma_y_RD = CalculateGLCMSTD_j(_p_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += (i - mu_x_H) * (j - mu_y_H) * _p_H[i][j] / (sigma_x_H * sigma_y_H);
            f_V += (i - mu_x_V) * (j - mu_y_V) * _p_V[i][j] / (sigma_x_V * sigma_y_V);
            f_LD += (i - mu_x_LD) * (j - mu_y_LD) * _p_LD[i][j] / (sigma_x_LD * sigma_y_LD);
            f_RD += (i - mu_x_RD) * (j - mu_y_RD) * _p_RD[i][j] / (sigma_x_RD * sigma_y_RD);
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetCorrelationII(Features& f) {
    // Calculate means
    double mu_x_H = CalculateMean(_px_H);
    double mu_x_V = CalculateMean(_px_V);
    double mu_x_LD = CalculateMean(_px_LD);
    double mu_x_RD = CalculateMean(_px_RD);

    double mu_y_H = CalculateMean(_py_H);
    double mu_y_V = CalculateMean(_py_V);
    double mu_y_LD = CalculateMean(_py_LD);
    double mu_y_RD = CalculateMean(_py_RD);

    // Calculate STDs
    double sigma_x_H = CalculateSTD(_px_H);
    double sigma_x_V = CalculateSTD(_px_V);
    double sigma_x_LD = CalculateSTD(_px_LD);
    double sigma_x_RD = CalculateSTD(_px_RD);

    double sigma_y_H = CalculateSTD(_py_H);
    double sigma_y_V = CalculateSTD(_py_V);
    double sigma_y_LD = CalculateSTD(_py_LD);
    double sigma_y_RD = CalculateSTD(_py_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += (i * j) * _p_H[i][j];
            f_V += (i * j) * _p_V[i][j];
            f_LD += (i * j) * _p_LD[i][j];
            f_RD += (i * j) * _p_RD[i][j];
        }
    }

    f_H = (f_H - (mu_x_H * mu_y_H)) / (sigma_x_H * sigma_y_H);
    f_V = (f_V - (mu_x_V * mu_y_V)) / (sigma_x_V * sigma_y_V);
    f_LD = (f_LD - (mu_x_LD * mu_y_LD)) / (sigma_x_LD * sigma_y_LD);
    f_RD = (f_RD - (mu_x_RD * mu_y_RD)) / (sigma_x_RD * sigma_y_RD);

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetCorrelationIIAnotherWay(Features& f) {
    // Calculate means
    double mu_x_H = CalculateGLCMMean_i(_p_H);
    double mu_x_V = CalculateGLCMMean_i(_p_V);
    double mu_x_LD = CalculateGLCMMean_i(_p_LD);
    double mu_x_RD = CalculateGLCMMean_i(_p_RD);

    double mu_y_H = CalculateGLCMMean_j(_p_H);
    double mu_y_V = CalculateGLCMMean_j(_p_V);
    double mu_y_LD = CalculateGLCMMean_j(_p_LD);
    double mu_y_RD = CalculateGLCMMean_j(_p_RD);

    // Calculate STDs
    double sigma_x_H = CalculateGLCMSTD_i(_p_H);
    double sigma_x_V = CalculateGLCMSTD_i(_p_V);
    double sigma_x_LD = CalculateGLCMSTD_i(_p_LD);
    double sigma_x_RD = CalculateGLCMSTD_i(_p_RD);

    double sigma_y_H = CalculateGLCMSTD_j(_p_H);
    double sigma_y_V = CalculateGLCMSTD_j(_p_V);
    double sigma_y_LD = CalculateGLCMSTD_j(_p_LD);
    double sigma_y_RD = CalculateGLCMSTD_j(_p_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += (i * j) * _p_H[i][j];
            f_V += (i * j) * _p_V[i][j];
            f_LD += (i * j) * _p_LD[i][j];
            f_RD += (i * j) * _p_RD[i][j];
        }
    }

    f_H = (f_H - (mu_x_H * mu_y_H)) / (sigma_x_H * sigma_y_H);
    f_V = (f_V - (mu_x_V * mu_y_V)) / (sigma_x_V * sigma_y_V);
    f_LD = (f_LD - (mu_x_LD * mu_y_LD)) / (sigma_x_LD * sigma_y_LD);
    f_RD = (f_RD - (mu_x_RD * mu_y_RD)) / (sigma_x_RD * sigma_y_RD);

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetCorrelationIII(Features& f) {
    // Calculate means
    double mu_x_H = CalculateMean(_px_H);
    double mu_x_V = CalculateMean(_px_V);
    double mu_x_LD = CalculateMean(_px_LD);
    double mu_x_RD = CalculateMean(_px_RD);

    double mu_y_H = CalculateMean(_py_H);
    double mu_y_V = CalculateMean(_py_V);
    double mu_y_LD = CalculateMean(_py_LD);
    double mu_y_RD = CalculateMean(_py_RD);

    // Calculate STDs
    double sigma_x_H = CalculateSTD(_px_H);
    double sigma_x_V = CalculateSTD(_px_V);
    double sigma_x_LD = CalculateSTD(_px_LD);
    double sigma_x_RD = CalculateSTD(_px_RD);

    double sigma_y_H = CalculateSTD(_py_H);
    double sigma_y_V = CalculateSTD(_py_V);
    double sigma_y_LD = CalculateSTD(_py_LD);
    double sigma_y_RD = CalculateSTD(_py_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += (i * j) * _p_H[i][j];
            f_V += (i * j) * _p_V[i][j];
            f_LD += (i * j) * _p_LD[i][j];
            f_RD += (i * j) * _p_RD[i][j];
        }
    }

    f_H = (f_H - (mu_x_H * mu_y_H)) / (sigma_x_H * sigma_y_H * sigma_x_H * sigma_y_H);
    f_V = (f_V - (mu_x_V * mu_y_V)) / (sigma_x_V * sigma_y_V * sigma_x_V * sigma_y_V);
    f_LD = (f_LD - (mu_x_LD * mu_y_LD)) / (sigma_x_LD * sigma_y_LD * sigma_x_LD * sigma_y_LD);
    f_RD = (f_RD - (mu_x_RD * mu_y_RD)) / (sigma_x_RD * sigma_y_RD * sigma_x_RD * sigma_y_RD);

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetSumOfSquares(Features& f) {
    double mean_H_x = CalculateGLCMMean_i(_p_H);
    double mean_V_x = CalculateGLCMMean_i(_p_V);
    double mean_LD_x = CalculateGLCMMean_i(_p_LD);
    double mean_RD_x = CalculateGLCMMean_i(_p_RD);

    double mean_H_y = CalculateGLCMMean_j(_p_H);
    double mean_V_y = CalculateGLCMMean_j(_p_V);
    double mean_LD_y = CalculateGLCMMean_j(_p_LD);
    double mean_RD_y = CalculateGLCMMean_j(_p_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += (i - mean_H_x) * (i - mean_H_x) * _p_H[i][j] + (j - mean_H_y) * (j - mean_H_y) * _p_H[i][j];
            f_V += (i - mean_V_x) * (i - mean_V_x) * _p_V[i][j] + (j - mean_V_y) * (j - mean_V_y) * _p_V[i][j];
            f_LD += (i - mean_LD_x) * (i - mean_LD_x) * _p_LD[i][j] + (j - mean_LD_y) * (j - mean_LD_y) * _p_LD[i][j];
            f_RD += (i - mean_RD_x) * (i - mean_RD_x) * _p_RD[i][j] + (j - mean_RD_y) * (j - mean_RD_y) * _p_RD[i][j];
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetSumOfSquares_i(Features& f) {
    double mean_H = CalculateGLCMMean_i(_p_H);
    double mean_V = CalculateGLCMMean_i(_p_V);
    double mean_LD = CalculateGLCMMean_i(_p_LD);
    double mean_RD = CalculateGLCMMean_i(_p_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += (i - mean_H) * (i - mean_H) * _p_H[i][j];
            f_V += (i - mean_V) * (i - mean_V) * _p_V[i][j];
            f_LD += (i - mean_LD) * (i - mean_LD) * _p_LD[i][j];
            f_RD += (i - mean_RD) * (i - mean_RD) * _p_RD[i][j];
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetSumOfSquares_j(Features& f) {
    double mean_H = CalculateGLCMMean_j(_p_H);
    double mean_V = CalculateGLCMMean_j(_p_V);
    double mean_LD = CalculateGLCMMean_j(_p_LD);
    double mean_RD = CalculateGLCMMean_j(_p_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += (j - mean_H) * (j - mean_H) * _p_H[i][j];
            f_V += (j - mean_V) * (j - mean_V) * _p_V[i][j];
            f_LD += (j - mean_LD) * (j - mean_LD) * _p_LD[i][j];
            f_RD += (j - mean_RD) * (j - mean_RD) * _p_RD[i][j];
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetHomogeneityII(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += _p_H[i][j] / (1 + (i - j) * (i - j));
            f_V += _p_V[i][j] / (1 + (i - j) * (i - j));
            f_LD += _p_LD[i][j] / (1 + (i - j) * (i - j));
            f_RD += _p_RD[i][j] / (1 + (i - j) * (i - j));
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetSumAverage(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < (2 * _Ng - 1); ++i) {
        f_H += i * _p_xpy_H[i];
        f_V += i * _p_xpy_V[i];
        f_LD += i * _p_xpy_LD[i];
        f_RD += i * _p_xpy_RD[i];
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetSumVariance(Features& f) {
    Features f8;
    GetSumEntropy(f8);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < (2 * _Ng - 1); ++i) {
        f_H += (i - f8.H) * (i - f8.H) * _p_xpy_H[i];
        f_V += (i - f8.V) * (i - f8.V) * _p_xpy_V[i];
        f_LD += (i - f8.LD) * (i - f8.LD) * _p_xpy_LD[i];
        f_RD += (i - f8.RD) * (i - f8.RD) * _p_xpy_RD[i];
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetSumEntropy(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < (2 * _Ng - 1); ++i) {
        if (_p_xpy_H[i] > 0) {
            f_H -= _p_xpy_H[i] * log(_p_xpy_H[i]);
        }
        if (_p_xpy_V[i] > 0) {
            f_V -= _p_xpy_V[i] * log(_p_xpy_V[i]);
        }
        if (_p_xpy_LD[i] > 0) {
            f_LD -= _p_xpy_LD[i] * log(_p_xpy_LD[i]);
        }
        if (_p_xpy_RD[i] > 0) {
            f_RD -= _p_xpy_RD[i] * log(_p_xpy_RD[i]);
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetEntropy(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            if (_p_H[i][j] > 0) {
                f_H -= _p_H[i][j] * log(_p_H[i][j]);
            }
            if (_p_V[i][j] > 0) {
                f_V -= _p_V[i][j] * log(_p_V[i][j]);
            }
            if (_p_LD[i][j] > 0) {
                f_LD -= _p_LD[i][j] * log(_p_LD[i][j]);
            }
            if (_p_RD[i][j] > 0) {
                f_RD -= _p_RD[i][j] * log(_p_RD[i][j]);
            }
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetDifferenceVariance(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        f_H += i * i * _p_xny_H[i];
        f_V += i * i * _p_xny_V[i];
        f_LD += i * i * _p_xny_LD[i];
        f_RD += i * i * _p_xny_RD[i];
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetDifferenceEntropy(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        if (_p_xny_H[i] > 0) {
            f_H -= _p_xny_H[i] * log(_p_xny_H[i]);
        }
        if (_p_xny_V[i] > 0) {
            f_V -= _p_xny_V[i] * log(_p_xny_V[i]);
        }
        if (_p_xny_LD[i] > 0) {
            f_LD -= _p_xny_LD[i] * log(_p_xny_LD[i]);
        }
        if (_p_xny_RD[i] > 0) {
            f_RD -= _p_xny_RD[i] * log(_p_xny_RD[i]);
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetInformationMeasuresOfCorrelation(Features& f1, Features& f2) {
    // calculate entropy factors
    CalculateHX();
    CalculateHY();
    CalculateHXY();
    CalculateHXY1();
    CalculateHXY2();

    // calculate the first Information Measures of Correlation
    double f1_H;
    double f1_V;
    double f1_LD;
    double f1_RD;

    f1_H = (_HXY_H - _HXY1_H) / std::max(_HX_H, _HY_H);
    f1_V = (_HXY_V - _HXY1_V) / std::max(_HX_V, _HY_V);
    f1_LD = (_HXY_LD - _HXY1_LD) / std::max(_HX_LD, _HY_LD);
    f1_RD = (_HXY_RD - _HXY1_RD) / std::max(_HX_RD, _HY_RD);

    f1(f1_H, f1_V, f1_LD, f1_RD);

    // calculate the second Information Measures of Correlation
    double f2_H = sqrt(1.0 - exp(-2.0 * (_HXY2_H - _HXY_H)));
    double f2_V = sqrt(1.0 - exp(-2.0 * (_HXY2_V - _HXY_V)));
    double f2_LD = sqrt(1.0 - exp(-2.0 * (_HXY2_LD - _HXY_LD)));
    double f2_RD = sqrt(1.0 - exp(-2.0 * (_HXY2_RD - _HXY_RD)));

    f2(f2_H, f2_V, f2_LD, f2_RD);
}

void TextureAnalysis::GetMaximalCorrelationCoefficient(Features& f) {
    // fill in Q matrices
    Eigen::MatrixXd Q_H(_Ng, _Ng);
    Eigen::MatrixXd Q_V(_Ng, _Ng);
    Eigen::MatrixXd Q_LD(_Ng, _Ng);
    Eigen::MatrixXd Q_RD(_Ng, _Ng);

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            Features q = CalculateQ(i, j);
            // std::cout << "q_H = " << q.H << ", q_V = " << q.V << ", q_LD = " << q.LD << ", q_RD = " << q.RD << std::endl;
            Q_H(i, j) = q.H;
            Q_V(i, j) = q.V;
            Q_LD(i, j) = q.LD;
            Q_RD(i, j) = q.RD;
        }
    }

    Eigen::EigenSolver<Eigen::MatrixXd> eigen_solver_Q_H(Q_H);
    Eigen::EigenSolver<Eigen::MatrixXd> eigen_solver_Q_V(Q_V);
    Eigen::EigenSolver<Eigen::MatrixXd> eigen_solver_Q_LD(Q_LD);
    Eigen::EigenSolver<Eigen::MatrixXd> eigen_solver_Q_RD(Q_RD);

    // get eigenvalues
    std::vector<double> eigens_H;
    std::vector<double> eigens_V;
    std::vector<double> eigens_LD;
    std::vector<double> eigens_RD;

    for (int i = 0; i < _Ng; ++i) {
        std::complex<double> E_H = eigen_solver_Q_H.eigenvalues().col(0)[i];
        std::complex<double> E_V = eigen_solver_Q_V.eigenvalues().col(0)[i];
        std::complex<double> E_LD = eigen_solver_Q_LD.eigenvalues().col(0)[i];
        std::complex<double> E_RD = eigen_solver_Q_RD.eigenvalues().col(0)[i];

        eigens_H.push_back(E_H.real());
        eigens_V.push_back(E_V.real());
        eigens_LD.push_back(E_LD.real());
        eigens_RD.push_back(E_RD.real());
    }

    // get second largest eigenvalues
    std::nth_element(eigens_H.begin(), eigens_H.begin() + 1, eigens_H.end(), std::greater<double>());
    std::nth_element(eigens_V.begin(), eigens_V.begin() + 1, eigens_V.end(), std::greater<double>());
    std::nth_element(eigens_LD.begin(), eigens_LD.begin() + 1, eigens_LD.end(), std::greater<double>());
    std::nth_element(eigens_RD.begin(), eigens_RD.begin() + 1, eigens_RD.end(), std::greater<double>());

    f(eigens_H[1], eigens_V[1], eigens_LD[1], eigens_RD[1]);
}

void TextureAnalysis::GetMean(Features& f) {
    double f_H = _pixel_values_mean;
    double f_V = _pixel_values_mean;
    double f_LD = _pixel_values_mean;
    double f_RD = _pixel_values_mean;
    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetStd(Features& f) {
    double f_H = _pixel_values_STD;
    double f_V = _pixel_values_STD;
    double f_LD = _pixel_values_STD;
    double f_RD = _pixel_values_STD;
    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetAutoCorrelation(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += i * j * _p_H[i][j];
            f_V += i * j * _p_V[i][j];
            f_LD += i * j * _p_LD[i][j];
            f_RD += i * j * _p_RD[i][j];
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetClusterProminence(Features& f) {
    // Calculate means
    double mu_x_H = CalculateMean(_px_H);
    double mu_x_V = CalculateMean(_px_V);
    double mu_x_LD = CalculateMean(_px_LD);
    double mu_x_RD = CalculateMean(_px_RD);

    double mu_y_H = CalculateMean(_py_H);
    double mu_y_V = CalculateMean(_py_V);
    double mu_y_LD = CalculateMean(_py_LD);
    double mu_y_RD = CalculateMean(_py_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += pow((i + j - mu_x_H - mu_y_H), 4) * _p_H[i][j];
            f_V += pow((i + j - mu_x_V - mu_y_V), 4) * _p_V[i][j];
            f_LD += pow((i + j - mu_x_LD - mu_y_LD), 4) * _p_LD[i][j];
            f_RD += pow((i + j - mu_x_RD - mu_y_RD), 4) * _p_RD[i][j];
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetClusterShade(Features& f) {
    // Calculate means
    double mu_x_H = CalculateMean(_px_H);
    double mu_x_V = CalculateMean(_px_V);
    double mu_x_LD = CalculateMean(_px_LD);
    double mu_x_RD = CalculateMean(_px_RD);

    double mu_y_H = CalculateMean(_py_H);
    double mu_y_V = CalculateMean(_py_V);
    double mu_y_LD = CalculateMean(_py_LD);
    double mu_y_RD = CalculateMean(_py_RD);

    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += pow((i + j - mu_x_H - mu_y_H), 3) * _p_H[i][j];
            f_V += pow((i + j - mu_x_V - mu_y_V), 3) * _p_V[i][j];
            f_LD += pow((i + j - mu_x_LD - mu_y_LD), 3) * _p_LD[i][j];
            f_RD += pow((i + j - mu_x_RD - mu_y_RD), 3) * _p_RD[i][j];
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetDissimilarity(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += fabs(i - j) * _p_H[i][j];
            f_V += fabs(i - j) * _p_V[i][j];
            f_LD += fabs(i - j) * _p_LD[i][j];
            f_RD += fabs(i - j) * _p_RD[i][j];
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetHomogeneityI(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += _p_H[i][j] / (1 + fabs(i - j));
            f_V += _p_V[i][j] / (1 + fabs(i - j));
            f_LD += _p_LD[i][j] / (1 + fabs(i - j));
            f_RD += _p_RD[i][j] / (1 + fabs(i - j));
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetMaximumProbability(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            if (_p_H[i][j] > f_H) {
                f_H = _p_H[i][j];
            }
            if (_p_V[i][j] > f_V) {
                f_V = _p_V[i][j];
            }
            if (_p_LD[i][j] > f_LD) {
                f_LD = _p_LD[i][j];
            }
            if (_p_RD[i][j] > f_RD) {
                f_RD = _p_RD[i][j];
            }
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetInverseDifferenceNormalized(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += _p_H[i][j] / (1 + (abs(i - j) * abs(i - j) / _Ng));
            f_V += _p_V[i][j] / (1 + (abs(i - j) * abs(i - j) / _Ng));
            f_LD += _p_LD[i][j] / (1 + (abs(i - j) * abs(i - j) / _Ng));
            f_RD += _p_RD[i][j] / (1 + (abs(i - j) * abs(i - j) / _Ng));
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

void TextureAnalysis::GetInverseDifferenceMomentNormalized(Features& f) {
    double f_H = 0.0;
    double f_V = 0.0;
    double f_LD = 0.0;
    double f_RD = 0.0;

    for (int i = 0; i < _Ng; ++i) {
        for (int j = 0; j < _Ng; ++j) {
            f_H += _p_H[i][j] / (1 + ((i - j) * (i - j) / _Ng));
            f_V += _p_V[i][j] / (1 + ((i - j) * (i - j) / _Ng));
            f_LD += _p_LD[i][j] / (1 + ((i - j) * (i - j) / _Ng));
            f_RD += _p_RD[i][j] / (1 + ((i - j) * (i - j) / _Ng));
        }
    }

    f(f_H, f_V, f_LD, f_RD);
}

std::map<Type, Features> TextureAnalysis::Calculate(const std::set<Type>& types) {
    std::map<Type, Features> results;
    bool information_measures_of_correlation_done = false;
    for (auto type : types) {
        switch (type) {
            case Type::Mean:
                GetMean(results[Type::Mean]);
                break;
            case Type::Std:
                GetStd(results[Type::Std]);
                break;
            case Type::AutoCorrelation:
                GetAutoCorrelation(results[Type::AutoCorrelation]);
                break;
            case Type::Contrast:
                GetContrast(results[Type::Contrast]);
                break;
            case Type::ContrastAnotherWay:
                GetContrastAnotherWay(results[Type::ContrastAnotherWay]);
                break;
            case Type::CorrelationI:
                GetCorrelationI(results[Type::CorrelationI]);
                break;
            case Type::CorrelationIAnotherWay:
                GetCorrelationIAnotherWay(results[Type::CorrelationIAnotherWay]);
                break;
            case Type::CorrelationII:
                GetCorrelationII(results[Type::CorrelationII]);
                break;
            case Type::CorrelationIIAnotherWay:
                GetCorrelationIIAnotherWay(results[Type::CorrelationIIAnotherWay]);
                break;
            case Type::CorrelationIII:
                GetCorrelationIII(results[Type::CorrelationIII]);
                break;
            case Type::ClusterProminence:
                GetClusterProminence(results[Type::ClusterProminence]);
                break;
            case Type::ClusterShade:
                GetClusterShade(results[Type::ClusterShade]);
                break;
            case Type::Dissimilarity:
                GetDissimilarity(results[Type::Dissimilarity]);
                break;
            case Type::Energy:
                GetEnergy(results[Type::Energy]);
                break;
            case Type::Entropy:
                GetEntropy(results[Type::Entropy]);
                break;
            case Type::HomogeneityI:
                GetHomogeneityI(results[Type::HomogeneityI]);
                break;
            case Type::HomogeneityII:
                GetHomogeneityII(results[Type::HomogeneityII]);
                break;
            case Type::MaximumProbability:
                GetMaximumProbability(results[Type::MaximumProbability]);
                break;
            case Type::SumOfSquares:
                GetSumOfSquares(results[Type::SumOfSquares]);
                break;
            case Type::SumOfSquaresI:
                GetSumOfSquares_i(results[Type::SumOfSquaresI]);
                break;
            case Type::SumOfSquaresJ:
                GetSumOfSquares_j(results[Type::SumOfSquaresJ]);
                break;
            case Type::SumAverage:
                GetSumAverage(results[Type::SumAverage]);
                break;
            case Type::SumEntropy:
                GetSumEntropy(results[Type::SumEntropy]);
                break;
            case Type::SumVariance:
                GetSumVariance(results[Type::SumVariance]);
                break;
            case Type::DifferenceVariance:
                GetDifferenceVariance(results[Type::DifferenceVariance]);
                break;
            case Type::DifferenceEntropy:
                GetDifferenceEntropy(results[Type::DifferenceEntropy]);
                break;
            case Type::InformationMeasuresOfCorrelationI:
                if (!information_measures_of_correlation_done) {
                    GetInformationMeasuresOfCorrelation(
                        results[Type::InformationMeasuresOfCorrelationI], results[Type::InformationMeasuresOfCorrelationII]);
                    information_measures_of_correlation_done = true;
                }
                break;
            case Type::InformationMeasuresOfCorrelationII:
                if (!information_measures_of_correlation_done) {
                    GetInformationMeasuresOfCorrelation(
                        results[Type::InformationMeasuresOfCorrelationI], results[Type::InformationMeasuresOfCorrelationII]);
                    information_measures_of_correlation_done = true;
                }
                break;
            case Type::InverseDifferenceNormalized:
                GetInverseDifferenceNormalized(results[Type::InverseDifferenceNormalized]);
                break;
            case Type::InverseDifferenceMomentNormalized:
                GetInverseDifferenceMomentNormalized(results[Type::InverseDifferenceMomentNormalized]);
                break;
            default:
                std::cerr << "Unknown feature type!\n";
                break;
        }
    }

    return results;
}

void TextureAnalysis::CalculateScore(double age, std::map<Type, Features>& features_map) {
    if ((age > 0) && features_map.count(Type::Mean) && features_map.count(Type::Entropy) && features_map.count(Type::Contrast)) {
        std::vector<double> params = {1.138, -1.814, 1.416, 1.714};

        //        std::cout << "Calculate the Score with parameters:\n";
        //        std::cout << "age = " << age << "\n";
        //        for (int i = 0; i < params.size(); ++i) {
        //            std::cout << "params[" << i << "] = " << params[i] << "\n";
        //        }

        double f_H = params[0] * age + params[1] * features_map.at(Type::Mean).H + params[2] * features_map.at(Type::Entropy).H +
                     params[3] * features_map.at(Type::Contrast).H;
        double f_V = params[0] * age + params[1] * features_map.at(Type::Mean).V + params[2] * features_map.at(Type::Entropy).V +
                     params[3] * features_map.at(Type::Contrast).V;
        double f_LD = params[0] * age + params[1] * features_map.at(Type::Mean).LD + params[2] * features_map.at(Type::Entropy).LD +
                      params[3] * features_map.at(Type::Contrast).LD;
        double f_RD = params[0] * age + params[1] * features_map.at(Type::Mean).RD + params[2] * features_map.at(Type::Entropy).RD +
                      params[3] * features_map.at(Type::Contrast).RD;

        features_map[Type::Score](f_H, f_V, f_LD, f_RD);
    } else {
        std::cerr << "Can not calculate the Score!\n";
    }
}

std::string TextureAnalysis::TypeToString(const Type& type) {
    std::string result;
    switch (type) {
        case Type::Mean:
            result = "Mean";
            break;
        case Type::Std:
            result = "STD";
            break;
        case Type::AutoCorrelation:
            result = "Auto Correlation";
            break;
        case Type::Contrast:
            result = "Contrast";
            break;
        case Type::ContrastAnotherWay:
            result = "Contrast (Check)";
            break;
        case Type::CorrelationI:
            result = "Correlation I";
            break;
        case Type::CorrelationIAnotherWay:
            result = "Correlation I (Check)";
            break;
        case Type::CorrelationII:
            result = "Correlation II";
            break;
        case Type::CorrelationIIAnotherWay:
            result = "Correlation II (Check)";
            break;
        case Type::CorrelationIII:
            result = "Correlation III";
            break;
        case Type::ClusterProminence:
            result = "Cluster Prominence";
            break;
        case Type::ClusterShade:
            result = "Cluster Shade";
            break;
        case Type::Dissimilarity:
            result = "Dissimilarity";
            break;
        case Type::Energy:
            result = "Energy";
            break;
        case Type::Entropy:
            result = "Entropy";
            break;
        case Type::HomogeneityI:
            result = "Homogeneity I";
            break;
        case Type::HomogeneityII:
            result = "Homogeneity II (Inverse Difference Moment)";
            break;
        case Type::MaximumProbability:
            result = "Maximum Probability";
            break;
        case Type::SumOfSquares:
            result = "Sum of Squares (in x and y)";
            break;
        case Type::SumOfSquaresI:
            result = "Sum of Squares (in x)";
            break;
        case Type::SumOfSquaresJ:
            result = "Sum of Squares (in y)";
            break;
        case Type::SumAverage:
            result = "Sum Average";
            break;
        case Type::SumEntropy:
            result = "Sum Entropy";
            break;
        case Type::SumVariance:
            result = "Sum Variance";
            break;
        case Type::DifferenceVariance:
            result = "Difference Variance";
            break;
        case Type::DifferenceEntropy:
            result = "Difference Entropy";
            break;
        case Type::InformationMeasuresOfCorrelationI:
            result = "Information Measures of Correlation I";
            break;
        case Type::InformationMeasuresOfCorrelationII:
            result = "Information Measures of Correlation II";
            break;
        case Type::InverseDifferenceNormalized:
            result = "Inverse Difference Normalized";
            break;
        case Type::InverseDifferenceMomentNormalized:
            result = "Inverse Difference Moment Normalized";
            break;
        case Type::Score:
            result = "Score";
            break;
        default:
            std::cerr << "Unknown feature type!\n";
            break;
    }
    return result;
}

std::string TextureAnalysis::DirectionToString(const Direction& direction) {
    std::string result;
    switch (direction) {
        case Direction::H:
            result = "H (0 deg)";
            break;
        case Direction::V:
            result = "V (90 deg)";
            break;
        case Direction::LD:
            result = "LD (135 deg)";
            break;
        case Direction::RD:
            result = "RD (45 deg)";
            break;
        case Direction::Avg:
            result = "Average";
            break;
        default:
            std::cerr << "Unknown feature type!\n";
            break;
    }

    return result;
}

void TextureAnalysis::Print(const std::map<Type, Features>& features) {
    for (auto feature : features) {
        std::cout << TypeToString(feature.first) << std::endl;
        std::cout << std::setw(30) << DirectionToString(Direction::H) << " = " << feature.second.H << std::endl;
        std::cout << std::setw(30) << DirectionToString(Direction::V) << " = " << feature.second.V << std::endl;
        std::cout << std::setw(30) << DirectionToString(Direction::LD) << " = " << feature.second.LD << std::endl;
        std::cout << std::setw(30) << DirectionToString(Direction::RD) << " = " << feature.second.RD << std::endl;
        std::cout << std::setw(30) << DirectionToString(Direction::Avg) << " = " << feature.second.Avg() << std::endl;
    }
}

void TextureAnalysis::SaveAsCSV(const std::string& image_name, std::map<Type, Features> features, const std::string& csv_name) {
    // check whether the csv file exists or not
    bool csv_file_exists = fs::exists(csv_name);

    // get image file base name
    std::string image_base_name = fs::path(image_name).filename().string();

    // get current time
    std::string current_time = GetCurrentTime();

    // open the csv file
    std::ofstream csv_file;
    csv_file.open(csv_name, std::ios::out | std::ios::app); // open as the writing mode and append the csv file at the end

    if (!csv_file) {
        std::cerr << "Can't the open file!" << std::endl;
        return;
    }

    if (!csv_file_exists) {
        // write a row of titles
        csv_file << "Date,";
        csv_file << "Image,";
        csv_file << "Direction,";
        for (std::map<Type, Features>::iterator it = features.begin(); it != features.end(); ++it) {
            csv_file << TypeToString(it->first) << ",";
        }
        csv_file << "\n";
    }

    // write a row of H values
    csv_file << current_time << ",";
    csv_file << image_base_name << ",";
    csv_file << DirectionToString(Direction::H) << ",";
    for (std::map<Type, Features>::iterator it = features.begin(); it != features.end(); ++it) {
        csv_file << it->second.H << ",";
    }
    csv_file << "\n";

    // write a row of V values
    csv_file << current_time << ",";
    csv_file << image_base_name << ",";
    csv_file << DirectionToString(Direction::V) << ",";
    for (std::map<Type, Features>::iterator it = features.begin(); it != features.end(); ++it) {
        csv_file << it->second.V << ",";
    }
    csv_file << "\n";

    // write a row of LD values
    csv_file << current_time << ",";
    csv_file << image_base_name << ",";
    csv_file << DirectionToString(Direction::LD) << ",";
    for (std::map<Type, Features>::iterator it = features.begin(); it != features.end(); ++it) {
        csv_file << it->second.LD << ",";
    }
    csv_file << "\n";

    // write a row of RD values
    csv_file << current_time << ",";
    csv_file << image_base_name << ",";
    csv_file << DirectionToString(Direction::RD) << ",";
    for (std::map<Type, Features>::iterator it = features.begin(); it != features.end(); ++it) {
        csv_file << it->second.RD << ",";
    }
    csv_file << "\n";

    // write a row of Avg values
    csv_file << current_time << ",";
    csv_file << image_base_name << ",";
    csv_file << DirectionToString(Direction::Avg) << ",";
    for (std::map<Type, Features>::iterator it = features.begin(); it != features.end(); ++it) {
        csv_file << it->second.Avg() << ",";
    }
    csv_file << "\n";

    // close the csv file
    csv_file.close();
}

std::string TextureAnalysis::GetCurrentTime() {
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    std::string str(buffer);

    return str;
}

void TextureAnalysis::CalculatePixelMean(const std::vector<double>& vec) {
    _pixel_values_mean = 0.0;
    for (int i = 0; i < vec.size(); ++i) {
        _pixel_values_mean += vec[i];
    }
    _pixel_values_mean /= vec.size();
}

void TextureAnalysis::CalculatePixelSTD(const std::vector<double>& vec) {
    CalculatePixelMean(vec);
    _pixel_values_STD = 0.0;
    for (int i = 0; i < vec.size(); ++i) {
        _pixel_values_STD += (vec[i] - _pixel_values_mean) * (vec[i] - _pixel_values_mean);
    }
    _pixel_values_STD = _pixel_values_STD / (vec.size() - 1.0);
    _pixel_values_STD = sqrt(_pixel_values_STD);
}
