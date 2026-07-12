#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
build_dir="${RADLIB_X86_64_GRUB_SLINT_BUILD_DIR:-${repo_root}/build/embedded/x86_64_grub_slint}"
display="${RADLIB_X86_QEMU_DISPLAY:-gtk}"
build=1
virtio_disk="${RADLIB_X86_VIRTIO_DISK:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --headless)
            display="none"
            shift
            ;;
        --display)
            display="$2"
            shift 2
            ;;
        --no-build)
            build=0
            shift
            ;;
        --virtio-disk)
            virtio_disk="$2"
            shift 2
            ;;
        *)
            echo "usage: $0 [--headless] [--display gtk|sdl|none] [--virtio-disk path] [--no-build]" >&2
            exit 2
            ;;
    esac
done

if [[ "${build}" == "1" ]]; then
    cmake -S "${repo_root}/tools/embedded/x86_64_grub_slint" -B "${build_dir}"
    cmake --build "${build_dir}" --target radixkernel_x86_64_grub_slint_iso -j"${RADLIB_BUILD_JOBS:-2}"
fi

if [[ -z "${virtio_disk}" && -f "${build_dir}/radix-rootfs.ext2" ]]; then
    virtio_disk="${build_dir}/radix-rootfs.ext2"
fi

qemu_args=(
    -m "${RADLIB_X86_QEMU_MEMORY:-512M}"
    -cdrom "${build_dir}/radixkernel-x86-64-grub-slint.iso"
    -serial stdio
    -display "${display}"
    -no-reboot
)

if [[ -n "${virtio_disk}" ]]; then
    qemu_args+=(
        -drive "if=none,id=radixdisk,format=raw,file=${virtio_disk}"
        -device "virtio-blk-pci,drive=radixdisk,disable-modern=on"
    )
fi

exec qemu-system-x86_64 "${qemu_args[@]}"
