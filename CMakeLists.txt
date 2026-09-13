cmake_minimum_required(VERSION 3.22)

project(GLCM-Texture-Analysis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_OSX_ARCHITECTURES "arm64")

find_package(OpenCV REQUIRED)

# No version pin: Eigen 3.3+ and 5.x both work, but Eigen's config only accepts the same major version
find_package(Eigen3 REQUIRED NO_MODULE)
message(STATUS "Eigen3 found: ${Eigen3_VERSION}")

include_directories(${CMAKE_SOURCE_DIR})

set(LIBS
        ${LIBS}
        ${OpenCV_LIBS}
        Eigen3::Eigen)

set(SOURCES
        ${SOURCES}
        analysis/TextureAnalysis.cpp
        controller/PolygonController.cpp
        controller/RectController.cpp
        viewer/Viewer.cpp)

set(MAIN_FILES
        ${MAIN_FILES}
        glcm-analysis)

foreach (MAIN_FILE IN LISTS MAIN_FILES)
    add_executable(${MAIN_FILE} ${MAIN_FILE}.cpp ${SOURCES})
    target_link_libraries(${MAIN_FILE} ${LIBS})
endforeach ()

add_executable(canvas-example canvas-example.cpp)
target_link_libraries(canvas-example ${LIBS})

# Unit tests for the GLCM math (built only when GoogleTest is installed)
find_package(GTest)

if (GTest_FOUND)
    enable_testing()
    add_executable(glcm-tests tests/TextureAnalysisTest.cpp analysis/TextureAnalysis.cpp)
    target_link_libraries(glcm-tests ${LIBS} GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(glcm-tests DISCOVERY_MODE PRE_TEST)
endif ()
