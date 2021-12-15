cmake_minimum_required(VERSION 3.8)

project(GLCM-Texture-Analysis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)

if (Eigen3_FOUND)
    INCLUDE_DIRECTORIES("${EIGEN3_INCLUDE_DIR}")
    message(STATUS "Eigen3 found: ${EIGEN3_INCLUDE_DIR}")
endif ()

set(LIBS
        ${LIBS}
        ${OpenCV_LIBS})

set(SOURCES
        ${SOURCES}
        GLCM/TextureAnalysis.cpp)

set(MAIN_FILES
        ${MAIN_FILES}
        glcm-polygon
        glcm-rectangle
        canvas)

foreach (MAIN_FILE IN LISTS MAIN_FILES)
    add_executable(${MAIN_FILE} ${MAIN_FILE}.cpp ${SOURCES})
    target_link_libraries(${MAIN_FILE} ${LIBS})
endforeach ()

