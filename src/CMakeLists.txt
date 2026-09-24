# Version and target name shown in the banner and stored in the image tag
set(XISO_VERSION_STRING "2.7.1 (01.11.14)")
if(WIN32)
    set(XISO_TARGET_NAME "win32")
elseif(APPLE)
    set(XISO_TARGET_NAME "macos-x")
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(XISO_TARGET_NAME "freebsd")
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    set(XISO_TARGET_NAME "openbsd")
else()
    set(XISO_TARGET_NAME "linux")
endif()

configure_file("${PROJECT_SOURCE_DIR}/include/xiso/version.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/xiso/version.hpp" @ONLY)

add_library(xiso STATIC
    binary_file.cpp
    content_source.cpp
    entry.cpp
    error.cpp
    fs_scan.cpp
    image_reader.cpp
    image_writer.cpp
    media_patch.cpp
    operations.cpp
)
add_library(xiso::xiso ALIAS xiso)

target_include_directories(xiso PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(xiso PUBLIC cxx_std_23)
xiso_enable_warnings(xiso)
