add_executable(RADixKernelTests
    radix_kernel_tests.cpp
)

target_compile_features(RADixKernelTests PRIVATE cxx_std_20)
target_link_libraries(RADixKernelTests PRIVATE RADixKernel)

add_test(NAME RADixKernelTests COMMAND RADixKernelTests)
