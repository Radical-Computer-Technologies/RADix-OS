#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
jobs="${RADLIB_BUILD_JOBS:-4}"

build_one() {
    local name="$1"
    local build_dir="$2"
    local autotest="$3"
    local autologin="$4"
    local nano="${5:-false}"
    local nano_variant="${6:-none}"
    local max_user_processes="${7:-128}"
    local artifact_dir="${repo_root}/artifacts/rad/x86_64-grub-terminal-${name}"

    cmake -S "${repo_root}/tools/embedded/x86_64_grub_slint" -B "${build_dir}" \
        -DRAD_X86_UI_PROFILE=terminal \
        -DRAD_X86_TERMINAL_AUTOTEST_NANO="${autotest}" \
        -DRAD_X86_TERMINAL_AUTOLOGIN="${autologin}" \
        -DRAD_RKCONFIG_TERMINAL_NANO="${nano}" \
        -DRAD_RKCONFIG_TERMINAL_NANO_VARIANT="${nano_variant}" \
        -DRAD_RKCONFIG_TERMINAL_VIM="${RAD_RKCONFIG_TERMINAL_VIM:-true}" \
        -DRAD_RKCONFIG_TERMINAL_VIM_VARIANT="${RAD_RKCONFIG_TERMINAL_VIM_VARIANT:-tiny}" \
        -DRAD_X86_MAX_USER_PROCESSES="${max_user_processes}" \
        -DRAD_RKCONFIG_KERNEL_MAX_TASKS="${RAD_RKCONFIG_KERNEL_MAX_TASKS:-128}" \
        -DRAD_RKCONFIG_KERNEL_MAX_PROCESSES="${RAD_RKCONFIG_KERNEL_MAX_PROCESSES:-128}" \
        -DRAD_RKCONFIG_KERNEL_TASK_STACK_BYTES="${RAD_RKCONFIG_KERNEL_TASK_STACK_BYTES:-524288}" \
        -DRAD_RKCONFIG_KERNEL_TASK_STACK_POLICY="${RAD_RKCONFIG_KERNEL_TASK_STACK_POLICY:-dynamic}"
    cmake --build "${build_dir}" --target radkernel_x86_64_grub_slint_iso -j"${jobs}"

    mkdir -p "${artifact_dir}"
    cp "${build_dir}/radkernel-x86-64-grub-terminal.iso" "${artifact_dir}/radkernel-x86-64-grub-terminal-${name}.iso"
    cp "${build_dir}/rad-rootfs.ext4" "${artifact_dir}/rad-rootfs-${name}.ext4"
    cp "${build_dir}/rad-fat32.img" "${artifact_dir}/rad-fat32-${name}.img"

    echo "${name}:"
    echo "  ISO:    ${build_dir}/radkernel-x86-64-grub-terminal.iso"
    echo "  rootfs: ${build_dir}/rad-rootfs.ext4"
    echo "  FAT32:  ${build_dir}/rad-fat32.img"
    echo "  artifacts: ${artifact_dir}"
}

build_one \
    "interactive" \
    "${repo_root}/build/embedded/x86_64_grub_terminal_interactive" \
    false \
    false \
    false \
    none \
    128

build_one \
    "smoke" \
    "${repo_root}/build/embedded/x86_64_grub_terminal_smoke" \
    true \
    true \
    false \
    none \
    128
