#ifndef GLCM_OUTLINE_HPP_
#define GLCM_OUTLINE_HPP_

// Internal to glcm_core: tracing region outlines along pixel edges, shared by roi/RegionSelection and roi/RoiOperations

#include <array>
#include <vector>

namespace glcm::outline_detail {

// Outline along the pixel edges of the region containing pixel (start_x, start_y), which must be the region's first
// pixel in raster order. `inside(x, y)` tells whether a pixel belongs to the region (false outside the image). The walk
// goes clockwise on screen with the region on its right, starting along the top edge of the first pixel, and returns the
// vertices where it turns. Only the boundary through the first pixel's top edge is traced (the outer one), so holes are
// ignored. With eight_connected, pixels touching at a corner belong to the same region (the walk turns left there and the
// outline touches itself at that corner); otherwise only pixels sharing an edge do.
template <typename Inside>
std::vector<std::array<double, 2>> TraceOutline(int start_x, int start_y, Inside inside, bool eight_connected) {
    // Directions clockwise on screen (y down): right, down, left, up
    static const int STEP_X[4] = {1, 0, -1, 0};
    static const int STEP_Y[4] = {0, 1, 0, -1};
    // Offsets from a vertex to the pixel ahead on the left and ahead on the right of each direction
    static const int LEFT_X[4] = {0, 0, -1, -1};
    static const int LEFT_Y[4] = {-1, 0, 0, -1};
    static const int RIGHT_X[4] = {0, -1, -1, 0};
    static const int RIGHT_Y[4] = {0, 0, -1, -1};

    std::vector<std::array<double, 2>> outline{{static_cast<double>(start_x), static_cast<double>(start_y)}};
    int x = start_x;
    int y = start_y;
    int direction = 0; // along the top edge of the first pixel, whose upper and left neighbours are outside
    do {
        x += STEP_X[direction];
        y += STEP_Y[direction];
        const bool left = inside(x + LEFT_X[direction], y + LEFT_Y[direction]);
        const bool right = inside(x + RIGHT_X[direction], y + RIGHT_Y[direction]);
        int next = (direction + 1) % 4;
        if (left && (eight_connected || right)) {
            next = (direction + 3) % 4;
        } else if (right) {
            next = direction;
        }
        if (next != direction) {
            if (x != start_x || y != start_y) {
                outline.push_back({static_cast<double>(x), static_cast<double>(y)});
            }
            direction = next;
        }
    } while (x != start_x || y != start_y || direction != 0);
    return outline;
}

} // namespace glcm::outline_detail

#endif // GLCM_OUTLINE_HPP_
