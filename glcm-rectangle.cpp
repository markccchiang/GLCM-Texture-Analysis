#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp> // selectROI is part of tracking API

#include "GLCM/TextureAnalysis.hpp"

using namespace std;
using namespace cv;

const int Ng = 256;
int d; // neighborhood distance

std::map<glcm::Type, glcm::Features> results; // GLCM calculation results

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
                            } else if ((m == k) && (n == l)) {
                                texture_analysis.PushPixelValue(i);
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

        // Set feature types to calculate
        std::set<glcm::Type> features{glcm::Type::AutoCorrelation, glcm::Type::Contrast, glcm::Type::ContrastAnotherWay,
            glcm::Type::CorrelationI, glcm::Type::CorrelationIAnotherWay, glcm::Type::CorrelationII, glcm::Type::CorrelationIIAnotherWay,
            glcm::Type::ClusterProminence, glcm::Type::ClusterShade, glcm::Type::Dissimilarity, glcm::Type::Energy, glcm::Type::Entropy,
            glcm::Type::HomogeneityI, glcm::Type::HomogeneityII, glcm::Type::MaximumProbability, glcm::Type::SumOfSquares,
            glcm::Type::SumOfSquaresI, glcm::Type::SumOfSquaresJ, glcm::Type::SumAverage, glcm::Type::SumEntropy, glcm::Type::SumVariance,
            glcm::Type::DifferenceVariance, glcm::Type::DifferenceEntropy, glcm::Type::InformationMeasuresOfCorrelationI,
            glcm::Type::InformationMeasuresOfCorrelationII, glcm::Type::InverseDifferenceNormalized,
            glcm::Type::InverseDifferenceMomentNormalized};

        // Clear the calculation results
        results.clear();

        // Re-calculate the features
        results = texture_analysis.Calculate(features);

        // Print results
        texture_analysis.Print(results);

        // Display Cropped Image
        if (im_crop.cols > 0 && im_crop.rows > 0) {
            imshow("Image", im_crop);
        } else {
            break;
        }

        // Check if ESC key was pressed
        if (cv::waitKey(20) == 27) {
            execution = false;
            break;
        }
    } // End of the while loop

    // Save results as the CSV format
    texture_analysis.SaveAsCSV(filename, results, "glcm-rectangle.csv");

    // Destroy all windows
    cv::destroyAllWindows();

    return 0;
}
