# Project options. Included before project() so vcpkg manifest features can still be selected.

option(XISO_BUILD_TESTS "Build the unit tests" OFF)
if(XISO_BUILD_TESTS)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()
