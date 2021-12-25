#include "analysis/TextureAnalysis.hpp"
#include "controller/RectController.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "Usage: ./glcm-analysis <file name>" << endl;
        return 1;
    }
    string filename = argv[1];

    RectController controller;
    controller.Run(filename);

    return 0;
}
