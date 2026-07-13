cmake_minimum_required(VERSION 3.21)

project(radixkernel_x86_64_grub_slint C CXX ASM)

find_program(GRUB_MKRESCUE_EXECUTABLE grub-mkrescue)
find_program(MFORMAT_EXECUTABLE mformat)
find_program(X86_64_LINUX_GNU_GCC_EXECUTABLE x86_64-linux-gnu-gcc)
find_program(MKFS_EXT2_EXECUTABLE mkfs.ext2)
find_program(MKFS_EXT4_EXECUTABLE mkfs.ext4)
find_program(MKFS_FAT_EXECUTABLE mkfs.fat)
find_program(DEBUGFS_EXECUTABLE debugfs)
if(NOT GRUB_MKRESCUE_EXECUTABLE OR NOT MFORMAT_EXECUTABLE)
    message(FATAL_ERROR "ISO packaging requires grub-mkrescue and mformat from mtools")
endif()
if(NOT X86_64_LINUX_GNU_GCC_EXECUTABLE OR NOT MKFS_EXT2_EXECUTABLE OR NOT MKFS_EXT4_EXECUTABLE OR NOT MKFS_FAT_EXECUTABLE OR NOT DEBUGFS_EXECUTABLE)
    message(FATAL_ERROR "RADix rootfs packaging requires x86_64-linux-gnu-gcc, mkfs.ext2, mkfs.ext4, mkfs.fat, and debugfs")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RADIX_OS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH "Path to RADix-OS")
if(NOT RADLIB_ROOT)
    get_filename_component(RADIX_OS_PARENT "${RADIX_OS_ROOT}/.." ABSOLUTE)
    set(RADLIB_ROOT "${RADIX_OS_PARENT}/RADLib" CACHE PATH "Path to RADLib")
endif()

set(RADLIB_EMBEDDED ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_FREESTANDING ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SOFTWARE ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_BACKEND_WINIT OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_BACKEND_QT OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_FEMTOVG OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SKIA OFF CACHE BOOL "" FORCE)
set(DEFAULT_SLINT_EMBED_RESOURCES embed-for-software-renderer CACHE STRING "" FORCE)
set(SLINT_SOURCE_DIR "${RADLIB_ROOT}/.radbuild/slint-src" CACHE PATH "Cached Slint source tree")
if(NOT EXISTS "${SLINT_SOURCE_DIR}/api/cpp/CMakeLists.txt")
    message(FATAL_ERROR "Slint source tree not found at ${SLINT_SOURCE_DIR}; clone Slint into .radbuild/slint-src")
endif()
if(DEFINED ENV{RUSTFLAGS})
    set(ENV{RUSTFLAGS} "$ENV{RUSTFLAGS} -C panic=abort")
else()
    set(ENV{RUSTFLAGS} "-C panic=abort")
endif()
add_subdirectory("${SLINT_SOURCE_DIR}/api/cpp" "${CMAKE_BINARY_DIR}/slint-cpp")
if(TARGET slint_cpp-static)
    set_property(TARGET slint_cpp-static PROPERTY INTERFACE_LINK_LIBRARIES "")
endif()

add_executable(radixkernel_x86_64_grub_slint
    boot.S
    x86_cpu.S
    kernel.cpp
    slint_shell.cpp
    x86_cpu.cpp
    x86_vm.cpp
    x86_storage.cpp
    x86_ext2.cpp
    x86_ext4.cpp
    x86_fat.cpp
    x86_user.cpp
    hal_x86_64.cpp
    runtime.cpp
    "${RADIX_OS_ROOT}/RADixKernel/boot/radboot.cpp"
    "${RADIX_OS_ROOT}/RADixKernel/subsystems/RADixOverlayRuntime.cpp"
    "${RADIX_OS_ROOT}/RADixKernel/core/RADixKernel_runtime.cpp"
    "${RADIX_OS_ROOT}/services/RADCompositor/RADCompositorCore.cpp"
)

target_include_directories(radixkernel_x86_64_grub_slint PRIVATE
    "${RADIX_OS_ROOT}"
    "${RADIX_OS_ROOT}/RADixKernel/headers"
    "${RADIX_OS_ROOT}/RADixKernel/boot"
    "${RADIX_OS_ROOT}/services/RADCompositor"
)

target_link_libraries(radixkernel_x86_64_grub_slint PRIVATE Slint::Slint)

SLINT_TARGET_SOURCES(radixkernel_x86_64_grub_slint
    "${RADIX_OS_ROOT}/services/RADCompositor/RADCompositor.slint"
)

target_compile_options(radixkernel_x86_64_grub_slint PRIVATE
    -m64 -ffreestanding -fno-pic -fno-pie -fno-rtti -fno-exceptions
    -fno-stack-protector -mno-red-zone -Wall -Wextra
)

target_compile_definitions(radixkernel_x86_64_grub_slint PRIVATE
    SLINT_FEATURE_FREESTANDING
    SLINT_BACKEND=1
    RADLIB_EMBEDDED_NO_CPP_WRAPPERS=1
    RADIX_KERNEL_BACKEND_x86_64_grub=1
    RADIX_KERNEL_MAX_DEVICES=32
    RADIX_KERNEL_TASK_STACK_BYTES=32768
)

target_link_options(radixkernel_x86_64_grub_slint PRIVATE
    -m64 -nostdlib -static -static-libgcc -no-pie
    -Wl,-T,${CMAKE_CURRENT_LIST_DIR}/linker.ld
    -Wl,--no-dynamic-linker
    -Wl,--build-id=none
    -lgcc
)

set_target_properties(radixkernel_x86_64_grub_slint PROPERTIES
    OUTPUT_NAME radixkernel-x86-64-grub-slint
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set(ISO_ROOT "${CMAKE_BINARY_DIR}/iso")
set(ISO_PATH "${CMAKE_BINARY_DIR}/radixkernel-x86-64-grub-slint.iso")
set(RADIX_INIT_ELF "${CMAKE_BINARY_DIR}/radix-init.elf")
set(RADIX_RADINIT_ELF "${CMAKE_BINARY_DIR}/radix-radinit.elf")
set(RADIX_RADSH_ELF "${CMAKE_BINARY_DIR}/radix-radsh.elf")
set(RADIX_SH_ELF "${CMAKE_BINARY_DIR}/radix-sh.elf")
set(RADIX_RADGFX_ELF "${CMAKE_BINARY_DIR}/radix-radgfx-smoke.elf")
set(RADIX_ROOTFS_DIR "${CMAKE_BINARY_DIR}/rootfs")
set(RADIX_EXT2_ROOTFS_IMAGE "${CMAKE_BINARY_DIR}/radix-rootfs.ext2")
set(RADIX_ROOTFS_IMAGE "${CMAKE_BINARY_DIR}/radix-rootfs.ext4")
set(RADIX_EXT4_WRITE_SEED "${CMAKE_BINARY_DIR}/radix-ext4-write-seed.txt")
set(RADIX_FAT_IMAGE "${CMAKE_BINARY_DIR}/radix-fat32.img")

add_custom_command(
    OUTPUT "${RADIX_INIT_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_INIT_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_init.S"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_init.S"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_RADSH_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_RADSH_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_radsh_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_radsh.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_radsh_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_radsh.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_RADINIT_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fomit-frame-pointer -mno-red-zone
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_RADINIT_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_radinit_entry.S"
            "${CMAKE_CURRENT_LIST_DIR}/user_radinit.c"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_radinit_entry.S" "${CMAKE_CURRENT_LIST_DIR}/user_radinit.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_SH_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_SH_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_sh.S"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_sh.S"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_RADGFX_ELF}"
    COMMAND "${X86_64_LINUX_GNU_GCC_EXECUTABLE}" -nostdlib -static -no-pie
            -Wl,-Ttext=0x40000000
            -Wl,--build-id=none
            -o "${RADIX_RADGFX_ELF}"
            "${CMAKE_CURRENT_LIST_DIR}/user_radgfx.S"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/user_radgfx.S"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RADIX_ROOTFS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E touch "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND truncate -s 32M "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${MKFS_EXT2_EXECUTABLE}" -q -F "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /bin" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /sbin" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/services" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log/radix" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /run" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADINIT_ELF} /bin/init" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADINIT_ELF} /sbin/radinit" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/radsh" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/rash" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/sh" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADGFX_ELF} /bin/radgfx-smoke" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/test_script.sh /bin/test.sh" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json /etc/radix/services/storage-root.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/userspace-shell.json /etc/radix/services/userspace-shell.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json /etc/radix/services/base-terminal.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json /etc/radix/services/network-smoke.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json /etc/radix/services/fatfs.json" "${RADIX_EXT2_ROOTFS_IMAGE}"
    DEPENDS "${RADIX_RADINIT_ELF}" "${RADIX_INIT_ELF}" "${RADIX_RADSH_ELF}" "${RADIX_SH_ELF}" "${RADIX_RADGFX_ELF}" "${CMAKE_CURRENT_LIST_DIR}/test_script.sh"
            "${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json" "${CMAKE_CURRENT_LIST_DIR}/services/userspace-shell.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json" "${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json"
    VERBATIM
)

add_custom_command(
    OUTPUT "${RADIX_ROOTFS_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${RADIX_ROOTFS_IMAGE}" "${RADIX_EXT4_WRITE_SEED}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RADIX_ROOTFS_DIR}"
    COMMAND /bin/sh -c "printf 'RADix ext4 write path seed padding for in-place write test\\n' > '${RADIX_EXT4_WRITE_SEED}'"
    COMMAND ${CMAKE_COMMAND} -E touch "${RADIX_ROOTFS_IMAGE}"
    COMMAND truncate -s 64M "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${MKFS_EXT4_EXECUTABLE}" -q -F -O extent,^has_journal,^64bit,^metadata_csum "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /bin" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /sbin" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /etc/radix/services" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /tmp" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /var/log/radix" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "mkdir /run" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADINIT_ELF} /bin/init" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADINIT_ELF} /sbin/radinit" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/radsh" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/rash" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADSH_ELF} /bin/sh" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_RADGFX_ELF} /bin/radgfx-smoke" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/test_script.sh /bin/test.sh" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json /etc/radix/services/storage-root.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/userspace-shell.json /etc/radix/services/userspace-shell.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json /etc/radix/services/base-terminal.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json /etc/radix/services/network-smoke.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json /etc/radix/services/fatfs.json" "${RADIX_ROOTFS_IMAGE}"
    COMMAND "${DEBUGFS_EXECUTABLE}" -w -R "write ${RADIX_EXT4_WRITE_SEED} /tmp/radwrite.txt" "${RADIX_ROOTFS_IMAGE}"
    DEPENDS "${RADIX_RADINIT_ELF}" "${RADIX_INIT_ELF}" "${RADIX_RADSH_ELF}" "${RADIX_SH_ELF}" "${RADIX_RADGFX_ELF}" "${CMAKE_CURRENT_LIST_DIR}/test_script.sh"
            "${CMAKE_CURRENT_LIST_DIR}/services/storage-root.json" "${CMAKE_CURRENT_LIST_DIR}/services/userspace-shell.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/base-terminal.json" "${CMAKE_CURRENT_LIST_DIR}/services/network-smoke.json"
            "${CMAKE_CURRENT_LIST_DIR}/services/fatfs.json"
    VERBATIM
)

add_custom_target(radix_rootfs ALL DEPENDS "${RADIX_ROOTFS_IMAGE}" "${RADIX_EXT2_ROOTFS_IMAGE}")

add_custom_command(
    OUTPUT "${RADIX_FAT_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${RADIX_FAT_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E touch "${RADIX_FAT_IMAGE}"
    COMMAND truncate -s 64M "${RADIX_FAT_IMAGE}"
    COMMAND "${MKFS_FAT_EXECUTABLE}" -F 32 -n RADFAT "${RADIX_FAT_IMAGE}"
    VERBATIM
)

add_custom_target(radix_test_volumes ALL DEPENDS "${RADIX_FAT_IMAGE}")

add_custom_command(
    OUTPUT "${ISO_PATH}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${ISO_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_ROOT}/boot/grub"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:radixkernel_x86_64_grub_slint>" "${ISO_ROOT}/boot/radixkernel-x86-64-grub-slint.elf"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/grub.cfg" "${ISO_ROOT}/boot/grub/grub.cfg"
    COMMAND "${GRUB_MKRESCUE_EXECUTABLE}" -o "${ISO_PATH}" "${ISO_ROOT}"
    DEPENDS radixkernel_x86_64_grub_slint "${CMAKE_CURRENT_LIST_DIR}/grub.cfg"
    VERBATIM
)

add_custom_target(radixkernel_x86_64_grub_slint_iso ALL DEPENDS "${ISO_PATH}" radix_rootfs radix_test_volumes)
