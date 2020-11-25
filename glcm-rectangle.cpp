#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp> // selectROI is part of tracking API

#include "GLCM/TextureAnalysis.hpp"

using namespace std;
using namespace cv;

const int Ng = 256;
int d; // neighborhood distance

void PrintResults(string title, glcm::Features f);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: ./glcm-rectangle <file name> <distance>" << endl;
        return 1;
    }

    // Set the image file name
    string filename = argv[1];

    // Set the distance
    if (argc == 3) {
        string distance = argv[2];
        d = stoi(distance);
    } else {
        d = 1;
    }

    // Read image
    Mat im = imread(filename, IMREAD_GRAYSCALE);
    // cout << "im = " << im << endl;

    // Set the bool execution
    volatile bool execution(true);

    // Initialize the texure analysis
    glcm::TextureAnalysis texture_analysis(Ng);

    // Select ROI repeatedly
    while (execution) {
        cout << "=================================================================================\n";
        // Select ROI
        bool from_center = false;
        bool show_crosshair = false;
        Rect2d r = selectROI(im, from_center, show_crosshair);

        // Check is the ROI region valid
        if (r.area() <= 0) {
            break;
        }

        cout << "\nROI corners: tl(" << r.tl().x << "," << r.tl().y << "), br(" << r.br().x << "," << r.br().y << ")" << endl;
        cout << "ROI width: " << r.size().width << ", high: " << r.size().height << ", area: " << r.area() << endl;

        // Clear the cache
        texture_analysis.ResetCache();

        // Crop image
        Mat im_crop = im(r);

        // Calculate matrices elements:
        // Central pixel coord (m ,n), where "m" is the row index, and "n" is the column index
        for (int m = 0; m < im_crop.rows; ++m) {
            for (int n = 0; n < im_crop.cols; ++n) {
                // Nearest neighborhood pixel coord (k ,l), where "k" is the row index, and "l" is the column index
                for (int k = m - d; k <= m + d; ++k) {
                    for (int l = n - d; l <= n + d; ++l) {
                        if ((k >= 0) && (l >= 0) && (k < im_crop.rows) && (l < im_crop.cols)) {
                            //// ToDo: need to check are pixel values in coordinates (m,n) and (k,l) nan or masked!!
                            int j = (int)(im_crop.at<uchar>(m, n)); // I(m,n)
                            int i = (int)(im_crop.at<uchar>(k, l)); // I(k,l)
                            if (((k - m) == 0) && (abs(l - n) == d)) {
                                texture_analysis.CountElemH(i, j);
                            } else if ((((k - m) == d) && ((l - n) == -d)) || (((k - m) == -d) && ((l - n) == d))) {
                                texture_analysis.CountElemRD(i, j);
                            } else if ((abs(k - m) == d) && (l - n == 0)) {
                                texture_analysis.CountElemV(i, j);
                            } else if ((((k - m) == d) && ((l - n) == d)) || (((k - m) == -d) && ((l - n) == -d))) {
                                texture_analysis.CountElemLD(i, j);
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
        texture_analysis.Normalization();

        glcm::Features f1;
        texture_analysis.GetEnergy(f1);
        PrintResults("1. Angular Second Moment:", f1);

        glcm::Features f2;
        texture_analysis.GetContrast(f2);
        PrintResults("2. Contrast:", f2);

        glcm::Features f3;
        texture_analysis.GetCorrelationII(f3);
        PrintResults("3. Correlation:", f3);

        glcm::Features f4;
        texture_analysis.GetSumOfSquares(f4);
        PrintResults("4: Sum of Squares: Variance:", f4);

        glcm::Features f5;
        texture_analysis.GetHomogeneityII(f5);
        PrintResults("5. Inverse Difference Moment:", f5);

        glcm::Features f6;
        texture_analysis.GetSumAverage(f6);
        PrintResults("6. Sum Average:", f6);

        glcm::Features f7;
        texture_analysis.GetSumVariance(f7);
        PrintResults("7. Sum Variance:", f7);

        glcm::Features f8;
        texture_analysis.GetSumEntropy(f8);
        PrintResults("8. Sum Entropy:", f8);

        glcm::Features f9;
        texture_analysis.GetEntropy(f9);
        PrintResults("9. Entropy:", f9);

        glcm::Features f10;
        texture_analysis.GetDifferenceVariance(f10);
        PrintResults("10. Difference Variance:", f10);

        glcm::Features f11;
        texture_analysis.GetDifferenceEntropy(f11);
        PrintResults("11. Difference Entropy:", f11);

        glcm::Features f12;
        glcm::Features f13;
        texture_analysis.GetInformationMeasuresOfCorrelation(f12, f13);
        PrintResults("12. Information Measures of Correlation:", f12);
        PrintResults("13. Information Measures of Correlation:", f13);

        // cout << "Calculating Maximal Correlation Coefficient...\n";
        // glcm::Features f14;
        // texture_analysis.GetMaximalCorrelationCoefficient(f14);
        // PrintResults("14. Maximal Correlation Coefficient:", f14);

        glcm::Features F_1;
        texture_analysis.GetAutoCorrelation(F_1);
        PrintResults("F1: Auto Correlation:", F_1);

        glcm::Features F_3;
        texture_analysis.GetCorrelationI(F_3);
        PrintResults("F3: Correlation I", F_3);

        glcm::Features F_5;
        texture_analysis.GetClusterProminence(F_5);
        PrintResults("F5: Cluster Prominence", F_5);

        glcm::Features F_6;
        texture_analysis.GetClusterShade(F_6);
        PrintResults("F6: Cluster Shade", F_6);

        glcm::Features F_7;
        texture_analysis.GetDissimilarity(F_7);
        PrintResults("F7: Dissimilarity", F_7);

        glcm::Features F_10;
        texture_analysis.GetHomogeneityI(F_10);
        PrintResults("F10: Homogeneity I", F_10);

        glcm::Features F_12;
        texture_analysis.GetMaximumProbability(F_12);
        PrintResults("F12: Maximum Probability", F_12);

        glcm::Features F_21;
        texture_analysis.GetInverseDifferenceNormalized(F_21);
        PrintResults("F21: Inverse Difference Normalized", F_21);

        glcm::Features F_22;
        texture_analysis.GetInverseDifferenceMomentNormalized(F_22);
        PrintResults("F22: Inverse Difference Moment Normalized", F_22);

        //
        // Display Cropped Image
        //
        if (im_crop.cols > 0 && im_crop.rows > 0) {
            imshow("Image", im_crop);
        } else {
            break;
        }

        //
        // Check if ESC key was pressed
        //
        if (cv::waitKey(20) == 27) {
            execution = false;
            break;
        }
    } // End of the while loop

    // Destroy all windows
    cv::destroyAllWindows();

    return 0;
}

void PrintResults(string title, glcm::Features f) {
    cout << title << endl;
    cout << std::setw(30) << "f1_H (0 deg) = " << f.H << endl;
    cout << std::setw(30) << "f1_V (90 deg) = " << f.V << endl;
    cout << std::setw(30) << "f1_LD (135 deg) = " << f.LD << endl;
    cout << std::setw(30) << "f1_RD (45 deg) = " << f.RD << endl;
    cout << std::setw(30) << "avg = " << f.Avg() << endl;
}
