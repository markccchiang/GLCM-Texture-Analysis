# Node-API addon wrapping glcm_core. Built with cmake-js (npm run build -w @glcm/native), which provides
# CMAKE_JS_INC, CMAKE_JS_SRC and CMAKE_JS_LIB.
cmake_minimum_required(VERSION 3.22)

project(glcm_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# The core library, without its GoogleTest suite
set(GLCM_BUILD_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../core ${CMAKE_CURRENT_BINARY_DIR}/glcm_core)

execute_process(
        COMMAND node -p "require('node-addon-api').include_dir"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE)
if (NOT NODE_ADDON_API_DIR)
    message(FATAL_ERROR "node-addon-api not found; run npm install at the repository root")
endif ()
get_filename_component(NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}" ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(glcm_native SHARED src/addon.cpp ${CMAKE_JS_SRC})
set_target_properties(glcm_native PROPERTIES PREFIX "" SUFFIX ".node")
target_include_directories(glcm_native PRIVATE ${CMAKE_JS_INC} ${NODE_ADDON_API_DIR})
target_link_libraries(glcm_native PRIVATE glcm_core ${CMAKE_JS_LIB})
target_compile_definitions(glcm_native PRIVATE NAPI_VERSION=8 NAPI_CPP_EXCEPTIONS NODE_ADDON_API_DISABLE_DEPRECATED)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(glcm_native PRIVATE -Wall -Wextra)
endif ()
