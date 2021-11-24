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
    cv::Mat image = imread(filename, IMREAD_GRAYSCALE);

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

        // Crop image
        cv::Mat image_crop = image(selected_roi);

        // Calculate matrices elements:
        texture_analysis.ProcessRectImage(image_crop, d);

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
