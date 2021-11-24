#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp> // selectROI is part of tracking API

#include "GLCM/TextureAnalysis.hpp"

using namespace std;
using namespace cv;

const int Ng = 256;
int d;      // neighborhood distance
double age; // age of a person

std::map<glcm::Type, glcm::Features> results; // GLCM calculation results

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 4) {
        cout << "Usage: ./glcm-polygon <file name> <distance> <age>" << endl;
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

    // set age
    if (argc == 4) {
        string age_str = argv[3];
        age = stod(age_str);
    } else {
        age = 0.0;
    }

    // Read image
    Mat image = imread(filename, IMREAD_GRAYSCALE);

    // Set the bool execution
    volatile bool execution(true);

    // Initialize the texture analysis
    glcm::TextureAnalysis texture_analysis(Ng, age);

    // Select ROI repeatedly
    while (execution) {
        cout << "=================================================================================\n";
        // Select ROI
        bool from_center = false;
        bool show_crosshair = false;
        Rect2d selected_roi = selectROI(image, from_center, show_crosshair);

        // Check is the ROI region valid
        if (selected_roi.area() <= 0) {
            break;
        }

        cout << "\nROI corners: tl(" << selected_roi.tl().x << "," << selected_roi.tl().y << "), br(" << selected_roi.br().x << ","
             << selected_roi.br().y << ")" << endl;
        cout << "ROI width: " << selected_roi.size().width << ", high: " << selected_roi.size().height << ", area: " << selected_roi.area()
             << endl;

        // Clear the cache
        texture_analysis.ResetCache();

        // Crop image
        Mat image_crop = image(selected_roi);

        // Calculate matrices elements:
        // Central pixel coord (m ,n), where "m" is the row index, and "n" is the column index
        for (int m = 0; m < image_crop.rows; ++m) {
            for (int n = 0; n < image_crop.cols; ++n) {
                // Nearest neighborhood pixel coord (k ,l), where "k" is the row index, and "l" is the column index
                for (int k = m - d; k <= m + d; ++k) {
                    for (int l = n - d; l <= n + d; ++l) {
                        if ((k >= 0) && (l >= 0) && (k < image_crop.rows) && (l < image_crop.cols)) {
                            //// ToDo: need to check are pixel values in coordinates (m,n) and (k,l) nan or masked!!
                            int j = (int)(image_crop.at<uchar>(m, n)); // I(m,n)
                            int i = (int)(image_crop.at<uchar>(k, l)); // I(k,l)
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
        std::set<glcm::Type> features_all{glcm::Type::AutoCorrelation, glcm::Type::Contrast, glcm::Type::ContrastAnotherWay,
            glcm::Type::CorrelationI, glcm::Type::CorrelationIAnotherWay, glcm::Type::CorrelationII, glcm::Type::CorrelationIIAnotherWay,
            glcm::Type::ClusterProminence, glcm::Type::ClusterShade, glcm::Type::Dissimilarity, glcm::Type::Energy, glcm::Type::Entropy,
            glcm::Type::HomogeneityI, glcm::Type::HomogeneityII, glcm::Type::MaximumProbability, glcm::Type::SumOfSquares,
            glcm::Type::SumOfSquaresI, glcm::Type::SumOfSquaresJ, glcm::Type::SumAverage, glcm::Type::SumEntropy, glcm::Type::SumVariance,
            glcm::Type::DifferenceVariance, glcm::Type::DifferenceEntropy, glcm::Type::InformationMeasuresOfCorrelationI,
            glcm::Type::InformationMeasuresOfCorrelationII, glcm::Type::InverseDifferenceNormalized,
            glcm::Type::InverseDifferenceMomentNormalized};

        std::set<glcm::Type> features{glcm::Type::Energy, glcm::Type::HomogeneityII, glcm::Type::Contrast, glcm::Type::SumOfSquares,
            glcm::Type::CorrelationIII, glcm::Type::Entropy, glcm::Type::ClusterShade, glcm::Type::ClusterProminence};

        // Clear the calculation results
        results.clear();

        // Re-calculate the features
        results = texture_analysis.Calculate(features);

        // Calculate Score
        texture_analysis.CalculateScore(results);

        // Print results
        texture_analysis.Print(results);

        // Display Cropped Image
        if (image_crop.cols > 0 && image_crop.rows > 0) {
            imshow("Image", image_crop);
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
