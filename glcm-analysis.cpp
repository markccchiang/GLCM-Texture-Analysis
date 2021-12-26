#include "analysis/TextureAnalysis.hpp"
#include "controller/PolygonController.hpp"
#include "controller/RectController.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: ./glcm-analysis <file name> <mode: polygon, rectangle>" << endl;
        return 1;
    }

    string filename = argv[1];
    string mode("rectangle");

    if (argc == 3) {
        mode = argv[2];
    }

    if (mode == "polygon") {
        polygon::PolygonController controller;
        controller.Run(filename);
    } else {
        RectController controller;
        controller.Run(filename);
    }

    return 0;
}
