/*
This demo showcases some components of cvui being used to control
the application of the Canny edge algorithm to a loaded image.

Code licensed under the MIT license
*/

#include <opencv2/opencv.hpp>

#define CVUI_IMPLEMENTATION

#include "cvui.h"

#define WINDOW_NAME "CVUI Canvas"

int main(int argc, const char* argv[]) {
    cv::Mat lena = cv::imread("../samples/Sample.jpg");
    cv::Mat frame = lena.clone();
    int low_threshold = 50;
    int high_threshold = 150;
    bool use_canny = false;
    int panel_width = 180;
    int panel_height = 180;
    int pad = 15;

    cvui::init(WINDOW_NAME);

    while (true) {
        cv::Mat canvas = cv::Mat::ones(cv::Size((frame.cols + panel_width + pad * 2), frame.rows), CV_8UC3);

        // Should we apply Canny edge?
        if (use_canny) {
            // Yes, we should apply it.
            cv::cvtColor(lena, frame, cv::COLOR_BGR2GRAY);
            cv::Canny(frame, frame, low_threshold, high_threshold, 3);
            cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);
        } else {
            // No, so just copy the original image to the displaying frame.
            lena.copyTo(frame);
        }

        // Render the settings window to house the checkbox
        // and the trackbars below.
        cvui::window(canvas, frame.cols + pad, 50, panel_width, panel_height, "Settings");

        // Checkbox to enable/disable the use of Canny edge
        cvui::checkbox(canvas, frame.cols + 15, 80, "Use Canny Edge", &use_canny);

        // Two trackbars to control the low and high threshold values
        // for the Canny edge algorithm.
        cvui::trackbar(canvas, frame.cols + 15, 110, 165, &low_threshold, 5, 150);
        cvui::trackbar(canvas, frame.cols + 15, 180, 165, &high_threshold, 80, 300);

        cvui::printf(canvas, std::lround(frame.cols + 15), std::lround(300), 1.5 * cvui::DEFAULT_FONT_SCALE, 0x00ff00, "Low threshold: %d",
            low_threshold);
        cvui::printf(canvas, std::lround(frame.cols + 15), std::lround(320), 1.5 * cvui::DEFAULT_FONT_SCALE, 0x00ff00, "High threshold: %d",
            high_threshold);

        // This function must be called *AFTER* all UI components. It does
        // all the behind the scenes magic to handle mouse clicks, etc.
        cvui::update();

        // Show everything on the screen
        frame.copyTo(canvas(cv::Rect(0, 0, frame.cols, frame.rows)));

        cv::imshow(WINDOW_NAME, canvas);

        // Check if ESC was pressed
        if (cv::waitKey(30) == 27) {
            break;
        }
    }

    return 0;
}
