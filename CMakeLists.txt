cmake_minimum_required(VERSION 3.22)

project(GLCM-Texture-Analysis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_OSX_ARCHITECTURES "arm64")

find_package(OpenCV REQUIRED)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)

if (Eigen3_FOUND)
    INCLUDE_DIRECTORIES("${EIGEN3_INCLUDE_DIR}")
    message(STATUS "Eigen3 found: ${EIGEN3_INCLUDE_DIR}")
endif ()

include_directories(${CMAKE_SOURCE_DIR})

set(LIBS
        ${LIBS}
        ${OpenCV_LIBS})

set(SOURCES
        ${SOURCES}
        analysis/TextureAnalysis.cpp
        controller/PolygonController.cpp
        controller/RectController.cpp
        viewer/Viewer.cpp)

set(MAIN_FILES
        ${MAIN_FILES}
        glcm-analysis
        glcm-polygon
        glcm-rectangle)

foreach (MAIN_FILE IN LISTS MAIN_FILES)
    add_executable(${MAIN_FILE} ${MAIN_FILE}.cpp ${SOURCES})
    target_link_libraries(${MAIN_FILE} ${LIBS})
endforeach ()

add_executable(canvas-example canvas-example.cpp)
target_link_libraries(canvas-example ${LIBS})
