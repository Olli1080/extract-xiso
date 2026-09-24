cmake_minimum_required(VERSION 3.25)

# Load options before project() so they can influence the vcpkg manifest features
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Options.cmake")

project(extract-xiso
    VERSION 2.7.1
    DESCRIPTION "Create, list, rewrite and extract Xbox ISO (xdvdfs) images"
    LANGUAGES CXX)

include(GNUInstallDirs)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CLI11 CONFIG REQUIRED)

add_subdirectory(src)
add_subdirectory(main)

if(XISO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
