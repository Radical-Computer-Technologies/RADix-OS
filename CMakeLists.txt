cmake_minimum_required(VERSION 3.15)

project(RADOS LANGUAGES C CXX)

option(RADPX_OS_BUILD_TESTS "Build RADPx-OS host tests" ON)

add_subdirectory(RADKernel)

if(RADPX_OS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
