find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(xiso_tests
    avl_tree_test.cpp
    endian_test.cpp
    entry_test.cpp
    image_test.cpp
    media_patch_test.cpp
    test_support.cpp
)
target_link_libraries(xiso_tests PRIVATE xiso::xiso GTest::gtest GTest::gtest_main)
xiso_enable_warnings(xiso_tests)

gtest_discover_tests(xiso_tests)
