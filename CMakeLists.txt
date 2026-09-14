cmake_minimum_required(VERSION 3.22)

project(GLCM-Texture-Analysis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra)
endif ()

enable_testing()

# Texture analysis library and its unit tests
add_subdirectory(core)

# Legacy OpenCV/cvui desktop application, removed in phase 3 of doc/ui-design-plan.md. It needs OpenCV's highgui and
# the contrib tracking module (for selectROI), so it can be switched off where those are not installed.
option(GLCM_BUILD_LEGACY_APP "Build the legacy OpenCV/cvui application" ON)

if (GLCM_BUILD_LEGACY_APP)
    find_package(OpenCV REQUIRED)

    add_executable(glcm-analysis
            glcm-analysis.cpp
            controller/PolygonController.cpp
            controller/RectController.cpp
            viewer/Viewer.cpp)
    target_include_directories(glcm-analysis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(glcm-analysis PRIVATE glcm_core ${OpenCV_LIBS})

    add_executable(canvas-example canvas-example.cpp)
    target_include_directories(canvas-example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(canvas-example PRIVATE ${OpenCV_LIBS})
endif ()
