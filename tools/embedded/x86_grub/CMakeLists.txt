cmake_minimum_required(VERSION 3.15)

project(radixkernel_x86_grub C CXX ASM)

find_program(GRUB_MKRESCUE_EXECUTABLE grub-mkrescue)
find_program(MFORMAT_EXECUTABLE mformat)
set(RAD_X86_CAN_PACKAGE_ISO ON)
if(NOT GRUB_MKRESCUE_EXECUTABLE OR NOT MFORMAT_EXECUTABLE)
    set(RAD_X86_CAN_PACKAGE_ISO OFF)
    message(WARNING "x86 ELF will build, but ISO packaging requires grub-mkrescue and mformat from mtools")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(radixkernel_x86_grub
    boot.S
    kernel.cpp
)

target_compile_options(radixkernel_x86_grub PRIVATE
    -m32 -ffreestanding -fno-pic -fno-pie -fno-rtti -fno-exceptions
    -Wall -Wextra
)

target_link_options(radixkernel_x86_grub PRIVATE
    -m32 -nostdlib -no-pie
    -Wl,-T,${CMAKE_CURRENT_LIST_DIR}/linker.ld
)

set_target_properties(radixkernel_x86_grub PROPERTIES
    OUTPUT_NAME radixkernel-x86-grub
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set(ISO_ROOT "${CMAKE_BINARY_DIR}/iso")
set(ISO_PATH "${CMAKE_BINARY_DIR}/radixkernel-x86-grub.iso")

if(RAD_X86_CAN_PACKAGE_ISO)
    add_custom_command(
        OUTPUT "${ISO_PATH}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${ISO_ROOT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_ROOT}/boot/grub"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:radixkernel_x86_grub>" "${ISO_ROOT}/boot/radixkernel-x86-grub.elf"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/grub.cfg" "${ISO_ROOT}/boot/grub/grub.cfg"
        COMMAND "${GRUB_MKRESCUE_EXECUTABLE}" -o "${ISO_PATH}" "${ISO_ROOT}"
        DEPENDS radixkernel_x86_grub "${CMAKE_CURRENT_LIST_DIR}/grub.cfg"
        VERBATIM
    )
    add_custom_target(radixkernel_x86_grub_iso ALL DEPENDS "${ISO_PATH}")
else()
    add_custom_target(radixkernel_x86_grub_iso
        COMMAND ${CMAKE_COMMAND} -E echo "Cannot package ISO: install grub-mkrescue and mtools/mformat"
        COMMAND ${CMAKE_COMMAND} -E false
        DEPENDS radixkernel_x86_grub
    )
endif()
