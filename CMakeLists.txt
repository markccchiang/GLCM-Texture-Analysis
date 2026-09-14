cmake_minimum_required(VERSION 3.22)

project(GLCM-Texture-Analysis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra)
endif ()

enable_testing()

# Texture analysis library and its unit tests. The web application uses it through bindings/node.
add_subdirectory(core)
