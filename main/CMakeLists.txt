add_executable(extract-xiso main.cpp)
target_link_libraries(extract-xiso PRIVATE xiso::xiso CLI11::CLI11)
xiso_enable_warnings(extract-xiso)

install(TARGETS extract-xiso RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
