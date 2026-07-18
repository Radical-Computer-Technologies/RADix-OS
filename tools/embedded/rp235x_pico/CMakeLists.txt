cmake_minimum_required(VERSION 3.15)

set(PICO_BOARD "${PICO_BOARD}" CACHE STRING "Pico board name")
if(NOT PICO_BOARD)
    set(PICO_BOARD pico2 CACHE STRING "Pico board name" FORCE)
endif()

set(PICO_PLATFORM "${PICO_PLATFORM}" CACHE STRING "Pico platform")
if(NOT PICO_PLATFORM)
    set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Pico platform" FORCE)
endif()

if(NOT DEFINED PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}" CACHE PATH "Path to pico-sdk")
    else()
        set(PICO_SDK_PATH "/media/jvincent/Kingspec512/SDKs/pico-sdk" CACHE PATH "Path to pico-sdk")
    endif()
endif()

include("${PICO_SDK_PATH}/pico_sdk_init.cmake")

project(radkernel_rp235x_smoke C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RADPX_OS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH "Path to RADPx-OS")
if(NOT RADLIB_ROOT)
    get_filename_component(RADPX_OS_PARENT "${RADPX_OS_ROOT}/.." ABSOLUTE)
    set(RADLIB_ROOT "${RADPX_OS_PARENT}/RADLib" CACHE PATH "Path to RADLib")
endif()

add_compile_definitions(
    RADLIB_LICENSE_ACCEPTED=1
    RADLIB_LICENSE_TIER_INTERNAL=1
    RADLIB_EDITION_COMMUNITY=1
    RADLIB_EDITION_REQUIRES_LICENSE_KEY=0
)

pico_sdk_init()

set(RAD_KERNEL_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(RAD_KERNEL_BACKEND rp235x_pico CACHE STRING "" FORCE)
set(RADLIB_EMBEDDED ON CACHE BOOL "" FORCE)

add_subdirectory("${RADPX_OS_ROOT}/RADKernel" "${CMAKE_BINARY_DIR}/RADKernel")
add_subdirectory("${RADLIB_ROOT}/RADCore" "${CMAKE_BINARY_DIR}/RADCore")
add_subdirectory("${RADLIB_ROOT}/RADDsp" "${CMAKE_BINARY_DIR}/RADDsp")
add_subdirectory("${RADLIB_ROOT}/RADSettings" "${CMAKE_BINARY_DIR}/RADSettings")
add_subdirectory("${RADLIB_ROOT}/RADLogging" "${CMAKE_BINARY_DIR}/RADLogging")
add_subdirectory("${RADLIB_ROOT}/RADSerial" "${CMAKE_BINARY_DIR}/RADSerial")
add_subdirectory("${RADLIB_ROOT}/RADDevice" "${CMAKE_BINARY_DIR}/RADDevice")

add_executable(radkernel_rp235x_smoke
    main.cpp
)

target_include_directories(radkernel_rp235x_smoke PRIVATE
    "${RADPX_OS_ROOT}/RADKernel/headers"
    "${RADPX_OS_ROOT}/RADKernel/boot"
)

target_link_libraries(radkernel_rp235x_smoke
    PRIVATE
        RADKernel
        RADCore
        RADDsp
        RADSettings
        RADLogging
        RADSerial
        RADDevice
        pico_stdlib
        hardware_i2c
        hardware_spi
        hardware_dma
)

pico_enable_stdio_usb(radkernel_rp235x_smoke 0)
pico_enable_stdio_uart(radkernel_rp235x_smoke 1)
pico_add_extra_outputs(radkernel_rp235x_smoke)
