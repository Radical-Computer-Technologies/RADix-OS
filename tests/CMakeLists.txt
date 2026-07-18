add_executable(RADKernelTests
    rad_kernel_tests.cpp
)

target_compile_features(RADKernelTests PRIVATE cxx_std_20)
target_link_libraries(RADKernelTests PRIVATE RADKernel)

# Run from the source root: the suite invokes repo-relative helpers (e.g.
# python3 tools/radoverlay_compile.py). Without this, ctest runs from the build
# tree and those relative paths fail (they resolve only when the binary is run
# from the repo root).
add_test(NAME RADKernelTests COMMAND RADKernelTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
