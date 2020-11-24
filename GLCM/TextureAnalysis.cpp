#include "TextureAnalysis.hpp"

#include <math.h>

#include <Eigen/Eigenvalues>
#include <algorithm>

using namespace glcm;

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

void TextureAnalysis::GetAngularSecondMoment(Features& f) {
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

void TextureAnalysis::GetCorrelation(Features& f) {
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

void TextureAnalysis::GetVariance(Features& f) {
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

void TextureAnalysis::GetInverseDifferenceMoment(Features& f) {
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