cmake_minimum_required(VERSION 3.15)

option(RAD_KERNEL_BUILD_STATIC "Build RADKernel as a static archive" OFF)
option(RAD_KERNEL_ENABLE_SD_FAT "Enable RADKernel SD/FatFs integration points" ON)
option(RAD_KERNEL_ENABLE_SDIO_PIO "Enable optional RP235x PIO SDIO integration point" OFF)
option(RAD_KERNEL_ENABLE_PSRAM "Register backend PSRAM regions when available" OFF)
option(RAD_KERNEL_ENABLE_RP2350_HSTX_DVI "Expose the RP2350 HSTX/DVI framebuffer backend scaffold" OFF)
set(RAD_KERNEL_BACKEND "linux_sim" CACHE STRING "RADKernel backend: linux_sim, rp235x_pico, circle_pi, bcm283x_pi, or zynqmp_zuboard_1cg")
set(RAD_KERNEL_SD_MODE "auto" CACHE STRING "RADKernel SD mode: auto, spi, or pio_sdio")
set_property(CACHE RAD_KERNEL_BACKEND PROPERTY STRINGS linux_sim rp235x_pico circle_pi bcm283x_pi zynqmp_zuboard_1cg)
set_property(CACHE RAD_KERNEL_SD_MODE PROPERTY STRINGS auto spi pio_sdio)

set(RAD_KERNEL_LIBRARY_TYPE SHARED)
if(RAD_KERNEL_BUILD_STATIC OR RADLIB_EMBEDDED)
    set(RAD_KERNEL_LIBRARY_TYPE STATIC)
endif()

set(RAD_KERNEL_SOURCES
    boot/radboot.cpp
    subsystems/RADOverlayRuntime.cpp
)

if(RAD_KERNEL_BACKEND STREQUAL "linux_sim")
    list(APPEND RAD_KERNEL_SOURCES
        core/RADKernel.cpp
        platforms/a53/radkernel_a53.cpp
    )
elseif(RAD_KERNEL_BACKEND STREQUAL "rp235x_pico")
    set(RAD_KERNEL_LIBRARY_TYPE STATIC)
    list(APPEND RAD_KERNEL_SOURCES
        core/RADKernel_runtime.cpp
        hal/rp235x_pico/radkernel_hal_rp235x.cpp
    )
elseif(RAD_KERNEL_BACKEND STREQUAL "circle_pi")
    set(RAD_KERNEL_LIBRARY_TYPE STATIC)
    list(APPEND RAD_KERNEL_SOURCES
        core/RADKernel_runtime.cpp
        hal/circle/radkernel_hal_circle.cpp
        hal/circle/radkernel_runtime_newlib.cpp
    )
elseif(RAD_KERNEL_BACKEND STREQUAL "bcm283x_pi")
    set(RAD_KERNEL_LIBRARY_TYPE STATIC)
    list(APPEND RAD_KERNEL_SOURCES
        core/RADKernel_runtime.cpp
        platforms/a53/radkernel_a53.cpp
        platforms/a53/bcm283x/radkernel_hal_bcm283x.cpp
        hal/circle/radkernel_runtime_newlib.cpp
    )
elseif(RAD_KERNEL_BACKEND STREQUAL "zynqmp_zuboard_1cg")
    set(RAD_KERNEL_LIBRARY_TYPE STATIC)
    list(APPEND RAD_KERNEL_SOURCES
        core/RADKernel_runtime.cpp
        platforms/a53/radkernel_a53.cpp
        platforms/a53/zynqmp/radkernel_hal_zynqmp.cpp
        hal/circle/radkernel_runtime_newlib.cpp
    )
else()
    message(FATAL_ERROR "Unsupported RAD_KERNEL_BACKEND '${RAD_KERNEL_BACKEND}'")
endif()

add_library(RADKernel ${RAD_KERNEL_LIBRARY_TYPE}
    ${RAD_KERNEL_SOURCES}
)

set(RAD_KERNEL_BUILD_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include")
set(RAD_KERNEL_BUILD_HEADER_DIR "${RAD_KERNEL_BUILD_INCLUDE_DIR}/radkernel")
file(MAKE_DIRECTORY "${RAD_KERNEL_BUILD_HEADER_DIR}")
file(GLOB RAD_KERNEL_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/headers/radkernel/*.h")
foreach(RAD_KERNEL_HEADER ${RAD_KERNEL_HEADERS})
    get_filename_component(RAD_KERNEL_HEADER_NAME "${RAD_KERNEL_HEADER}" NAME)
    configure_file(
        "${RAD_KERNEL_HEADER}"
        "${RAD_KERNEL_BUILD_HEADER_DIR}/${RAD_KERNEL_HEADER_NAME}"
        COPYONLY
    )
endforeach()
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/boot/radboot.h"
    "${RAD_KERNEL_BUILD_INCLUDE_DIR}/radboot.h"
    COPYONLY
)

target_compile_features(RADKernel PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(RADKernel PRIVATE /O2 /MP /W4)
elseif(RAD_KERNEL_BACKEND STREQUAL "linux_sim")
    target_compile_options(RADKernel PRIVATE -O3 -Wall -Wextra -pthread -fPIC)
    target_link_libraries(RADKernel PUBLIC pthread)
else()
    target_compile_options(RADKernel PRIVATE -Os -Wall -Wextra -ffunction-sections -fdata-sections)
endif()

target_compile_definitions(RADKernel
    PUBLIC
        RAD_KERNEL_BACKEND_${RAD_KERNEL_BACKEND}=1
        RAD_KERNEL_ENABLE_SD_FAT=$<BOOL:${RAD_KERNEL_ENABLE_SD_FAT}>
        RAD_KERNEL_ENABLE_SDIO_PIO=$<BOOL:${RAD_KERNEL_ENABLE_SDIO_PIO}>
        RAD_KERNEL_ENABLE_PSRAM=$<BOOL:${RAD_KERNEL_ENABLE_PSRAM}>
        RAD_KERNEL_ENABLE_RP2350_HSTX_DVI=$<BOOL:${RAD_KERNEL_ENABLE_RP2350_HSTX_DVI}>
        RAD_KERNEL_SD_MODE="${RAD_KERNEL_SD_MODE}"
)

if(NOT RAD_KERNEL_BACKEND STREQUAL "linux_sim")
    target_compile_definitions(RADKernel PUBLIC RADLIB_EMBEDDED_NO_CPP_WRAPPERS=1)
endif()

target_include_directories(RADKernel
    PUBLIC
        "$<BUILD_INTERFACE:${RAD_KERNEL_BUILD_INCLUDE_DIR}>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/headers"
        "${CMAKE_CURRENT_SOURCE_DIR}/boot"
        "${CMAKE_CURRENT_SOURCE_DIR}/platforms/a53"
)

if(RAD_KERNEL_BACKEND STREQUAL "rp235x_pico" AND DEFINED PICO_SDK_PATH)
    target_include_directories(RADKernel PRIVATE
        "${PICO_SDK_PATH}/src/common/pico_base_headers/include"
        "${PICO_SDK_PATH}/src/rp2_common/pico_stdio/include"
        "${PICO_SDK_PATH}/src/rp2_common/pico_stdio_uart/include"
        "${PICO_SDK_PATH}/src/rp2_common/pico_multicore/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_timer/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_gpio/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_i2c/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_spi/include"
        "${PICO_SDK_PATH}/src/rp2_common/hardware_dma/include"
        "${PICO_SDK_PATH}/src/rp2350/hardware_regs/include"
    )
endif()

if(RAD_KERNEL_BACKEND STREQUAL "rp235x_pico" AND TARGET pico_stdlib)
    target_link_libraries(RADKernel PUBLIC pico_stdlib pico_multicore hardware_gpio hardware_i2c hardware_spi hardware_dma)
endif()

if(RAD_KERNEL_BACKEND STREQUAL "circle_pi" AND DEFINED RADLIB_CIRCLE_ROOT)
    target_include_directories(RADKernel PRIVATE
        "${RADLIB_CIRCLE_ROOT}/include"
    )
endif()
