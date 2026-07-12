#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

if [[ -d "${HOME}/snap/rustup/common/cargo/bin" ]]; then
    export PATH="${HOME}/snap/rustup/common/cargo/bin:${PATH}"
fi
rust_cmake_args=()
rust_toolchain_bin="${RADLIB_RUST_TOOLCHAIN_BIN:-${HOME}/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin}"
if [[ -x "${rust_toolchain_bin}/rustc" && -x "${rust_toolchain_bin}/cargo" ]]; then
    export PATH="${rust_toolchain_bin}:${PATH}"
    rust_cmake_args+=(
        "-DRust_COMPILER=${rust_toolchain_bin}/rustc"
        "-DRust_CARGO=${rust_toolchain_bin}/cargo"
    )
fi

slint_sdk="${SLINT_SDK_DIR:-/media/jvincent/Kingspec512/SDKs/Slint-cpp-1.17.0-Linux-x86_64}"
log_dir="${RADLIB_SMOKE_LOG_DIR:-${repo_root}/.radbuild}"
shell_build="${RADLIB_RAD_OS_SHELL_BUILD_DIR:-${repo_root}/build/embedded/rad_os_shell}"
x86_build="${RADLIB_X86_64_GRUB_SLINT_BUILD_DIR:-${repo_root}/build/embedded/x86_64_grub_slint}"
qemu_timeout="${RADLIB_X86_QEMU_TIMEOUT:-60s}"
qemu_smp="${RADLIB_X86_QEMU_SMP:-2}"

mkdir -p "${log_dir}"

if [[ -d "${slint_sdk}" && "${RADLIB_SKIP_HOST_SLINT_SMOKE:-0}" != "1" ]]; then
    cmake -S "${repo_root}/tools/embedded/rad_os_shell" -B "${shell_build}" \
        -DRADUI_SLINT_PROVIDER=binary \
        -DSLINT_SDK_DIR="${slint_sdk}"
    cmake --build "${shell_build}" --target rad_os_shell -j"${RADLIB_BUILD_JOBS:-2}"
    "${shell_build}/rad_os_shell" --self-test | tee "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RAD OS Slint shell rendered checksum=0x" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_TERMINAL_LOADING_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_TERMINAL_READY_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_WM_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_APP_TERMINAL_WINDOW_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_MENU_OPEN_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_MENU_ESCAPE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_WINDOW_MOVE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_WINDOW_RESIZE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_TERMINAL_CLOSE_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
    grep -q "RADIX_SLINT_TERMINAL_RELAUNCH_OK" "${log_dir}/rad-os-shell-slint-smoke.log"
else
    echo "Skipping hosted Slint smoke; set SLINT_SDK_DIR or unset RADLIB_SKIP_HOST_SLINT_SMOKE."
fi

cmake -S "${repo_root}/tools/embedded/x86_64_grub_slint" -B "${x86_build}" "${rust_cmake_args[@]}"
rm -f "${x86_build}/radix-rootfs.ext4" "${x86_build}/radix-fat32.img"
cmake --build "${x86_build}" --target radixkernel_x86_64_grub_slint_iso -j"${RADLIB_BUILD_JOBS:-2}"
grub-file --is-x86-multiboot2 "${x86_build}/radixkernel-x86-64-grub-slint"

qemu_log="${log_dir}/x86-64-grub-slint-smoke.log"
monitor_sock="${log_dir}/x86-64-grub-slint-monitor.sock"
rootfs_img="${x86_build}/radix-rootfs.ext4"
fat_img="${x86_build}/radix-fat32.img"
rm -f "${qemu_log}" "${monitor_sock}"
set +e
timeout "${qemu_timeout}" qemu-system-x86_64 \
    -m "${RADLIB_X86_QEMU_MEMORY:-512M}" \
    -smp "${qemu_smp}" \
    -cdrom "${x86_build}/radixkernel-x86-64-grub-slint.iso" \
    -drive "if=none,id=radixdisk,format=raw,file=${rootfs_img}" \
    -device "virtio-blk-pci,drive=radixdisk,disable-modern=on" \
    -drive "if=none,id=radixfat,format=raw,file=${fat_img}" \
    -device "virtio-blk-pci,drive=radixfat,disable-modern=on" \
    -netdev "user,id=radixnet" \
    -device "virtio-net-pci,netdev=radixnet,disable-modern=on" \
    -serial "file:${qemu_log}" \
    -monitor "unix:${monitor_sock},server,nowait" \
    -display none \
    -no-reboot &
qemu_pid=$!
keyboard_injected=0
for _ in $(seq 1 80); do
    [[ -S "${monitor_sock}" ]] && break
    sleep 0.1
done
for _ in $(seq 1 "${RADLIB_X86_INPUT_READY_POLLS:-140}"); do
    grep -q "RAD x86_64 Slint terminal ready" "${qemu_log}" 2>/dev/null && break
    sleep 0.1
done
sleep "${RADLIB_X86_KEYBOARD_INJECT_DELAY:-0.2}"
if [[ -S "${monitor_sock}" ]] && command -v socat >/dev/null 2>&1; then
    printf 'sendkey h\nsendkey e\nsendkey l\nsendkey p\nsendkey ret\n' \
        | socat - "UNIX-CONNECT:${monitor_sock}" >/dev/null 2>&1
    printf 'mouse_move 24 18\nmouse_button 1\nmouse_button 0\n' \
        | socat - "UNIX-CONNECT:${monitor_sock}" >/dev/null 2>&1
    for _ in $(seq 1 "${RADLIB_X86_INPUT_OBSERVED_POLLS:-30}"); do
        if grep -q "RADIX_IRQ_KEYBOARD_OK" "${qemu_log}" 2>/dev/null; then
            keyboard_injected=1
            break
        fi
        sleep 0.1
    done
fi
wait "${qemu_pid}"
qemu_status=$?
set -e
cat "${qemu_log}"

if [[ "${qemu_status}" != "0" && "${qemu_status}" != "124" ]]; then
    echo "QEMU exited with status ${qemu_status}" >&2
    exit "${qemu_status}"
fi

grep -q "RAD x86_64 GRUB Slint handoff" "${qemu_log}"
grep -q "RAD x86_64 Slint terminal ready" "${qemu_log}"
grep -q "RADIX_SLINT_BOOT_SHELL_OK" "${qemu_log}"
grep -q "RADIX_SLINT_TERMINAL_LOADING_OK" "${qemu_log}"
grep -q "RADIX_SLINT_TERMINAL_READY_OK" "${qemu_log}"
grep -q "RADIX_SLINT_WM_OK" "${qemu_log}"
grep -q "RADIX_SLINT_APP_TERMINAL_WINDOW_OK" "${qemu_log}"
grep -q "RADIX_SLINT_MENU_OPEN_OK" "${qemu_log}"
grep -q "RADIX_SLINT_MENU_ESCAPE_OK" "${qemu_log}"
grep -q "RADIX_SLINT_WINDOW_MOVE_OK" "${qemu_log}"
grep -q "RADIX_SLINT_WINDOW_RESIZE_OK" "${qemu_log}"
grep -q "RADIX_SLINT_TERMINAL_CLOSE_OK" "${qemu_log}"
grep -q "RADIX_SLINT_TERMINAL_RELAUNCH_OK" "${qemu_log}"
grep -q "RADIX_X86_CPU_OK" "${qemu_log}"
grep -q "RADIX_IRQ_CORE_OK" "${qemu_log}"
grep -q "RADIX_PIC_REMAP_OK" "${qemu_log}"
grep -q "RADIX_IRQ_DOMAIN_PIC_OK" "${qemu_log}"
grep -q "RADIX_IRQ_TREE_OK" "${qemu_log}"
grep -q "RADIX_TIMER_IRQ_OK" "${qemu_log}"
grep -q "RADIX_IDLE_HLT_OK" "${qemu_log}"
grep -q "RADIX_DEFERRED_WORK_OK" "${qemu_log}"
grep -q "RADIX_WAIT_QUEUE_OK" "${qemu_log}"
grep -q "RADIX_SCHED_CORE_OK" "${qemu_log}"
grep -q "RADIX_SMP_TOPOLOGY_OK" "${qemu_log}"
grep -q "RADIX_AP_START_OK" "${qemu_log}"
grep -q "RADIX_AP_WORKER_TASK_OK" "${qemu_log}"
grep -q "RADIX_CONTEXT_DISPATCH_OK" "${qemu_log}"
grep -q "RADIX_CONTEXT_SWITCH_OK" "${qemu_log}"
grep -q "RADIX_PREEMPT_SCHED_OK" "${qemu_log}"
if grep -q "RADIX_[A-Z0-9_]*_DEFERRED\\|RADIX_[A-Z0-9_]*_TODO" "${qemu_log}"; then
    echo "x86 completion gate failed: deferred/TODO marker present" >&2
    exit 1
fi
grep -q "RADIX_USER_IF_OK" "${qemu_log}"
grep -q "RADIX_CPP_RUNTIME_OK" "${qemu_log}"
grep -q "RADIX_MODULE_LIFECYCLE_OK" "${qemu_log}"
grep -q "RADIX_INT80_OK" "${qemu_log}"
grep -q "RADIX_POSIX_ABI_OK" "${qemu_log}"
grep -q "RADIX_VM_READY" "${qemu_log}"
grep -q "RADIX_VM_PAGE_ALLOC_OK" "${qemu_log}"
grep -q "RADIX_USER_VM_SCAFFOLD_OK" "${qemu_log}"
grep -q "RADIX_USER_VM_ISOLATED_OK" "${qemu_log}"
grep -q "RADIX_USER_ENTRY_MAP_OK" "${qemu_log}"
grep -q "RADIX_USER_COPY_OK" "${qemu_log}"
grep -q "RADIX_USER_ENTRY_PER_CORE_OK" "${qemu_log}"
grep -q "RADIX_USER_EXECVE_OK" "${qemu_log}"
grep -q "RADIX_USER_EXECVE_REENTER_OK" "${qemu_log}"
grep -q "RADIX_USER_PROCESS_SPAWN_OK" "${qemu_log}"
grep -q "RADIX_USER_PROCESS_WAIT_OK" "${qemu_log}"
grep -q "RADIX_USER_PROCESS_FD_TABLE_OK" "${qemu_log}"
grep -q "RADIX_USER_PROCESS_FD_NAMESPACE_OK" "${qemu_log}"
grep -q "RADIX_USER_FD_CLOEXEC_OK" "${qemu_log}"
grep -q "RADIX_USER_SYSCALLS_OK" "${qemu_log}"
grep -q "RADIX_USER_INVALID_PTR_OK" "${qemu_log}"
grep -q "RADIX_USER_LINUX_SYSCALL_ABI_OK" "${qemu_log}"
grep -q "RADIX_USER_RADSH_BOOT_OK" "${qemu_log}"
grep -q "RADIX_USER_RADSH_EXIT_OK" "${qemu_log}"
grep -q "RADIX_SLINT_APP_LAUNCH_PROCESS_OK" "${qemu_log}"
grep -q "RADIX_SLINT_APP_LIVE_PTY_OK" "${qemu_log}"
grep -q "RADIX_SLINT_WM_PTY_TERMINAL_OK" "${qemu_log}"
grep -q "RADIX_USER_FORK_OK" "${qemu_log}"
grep -q "RADIX_USER_FORK_CHILD_OK" "${qemu_log}"
grep -q "RADIX_USER_FORK_WAIT_OK" "${qemu_log}"
grep -q "RADIX_USER_FORK_FD_INHERIT_OK" "${qemu_log}"
grep -q "RADIX_USER_PIPE_FORK_OK" "${qemu_log}"
grep -q "RADIX_USER_COW_PAGE_FAULT_OK" "${qemu_log}"
grep -q "RADIX_USER_COW_PARENT_ISOLATED_OK" "${qemu_log}"
grep -q "RADIX_USER_WAIT_WAKE_OK" "${qemu_log}"
grep -q "RADIX_USER_ZOMBIE_REAP_OK" "${qemu_log}"
grep -q "RADIX_USER_SCRIPT_SHEBANG_OK" "${qemu_log}"
grep -q "RADIX_USER_ARGV_ENVP_OK" "${qemu_log}"
grep -q "RADIX_USER_SH_SCRIPT_OK" "${qemu_log}"
if grep -q "RADIX_USER_FORK_FAIL\\|RADIX_USER_COW_FAIL\\|RADIX_USER_SH_SCRIPT_FAIL" "${qemu_log}"; then
    echo "x86 fork/COW/script gate failed: failure marker present" >&2
    exit 1
fi
if grep -q "RADIX_USER_ENTRY_MAP_FAIL" "${qemu_log}"; then
    echo "x86 user VM gate failed: entry mapping failure marker present" >&2
    exit 1
fi
grep -q "RADIX_PCI_PROBE_OK" "${qemu_log}"
grep -q "RADIX_INPUT_POINTER_OK" "${qemu_log}"
grep -q "RADIX_VIRTIO_BLK_FOUND" "${qemu_log}"
grep -q "RADIX_VIRTIO_BLK_READ_OK" "${qemu_log}"
grep -q "RADIX_VIRTIO_NET_FOUND" "${qemu_log}"
grep -q "RADIX_NET_DEVICE_REGISTERED" "${qemu_log}"
grep -q "RADIX_ETH_TX_OK" "${qemu_log}"
grep -q "RADIX_ARP_OK" "${qemu_log}"
grep -q "RADIX_IPV4_OK" "${qemu_log}"
grep -q "RADIX_UDP_OK" "${qemu_log}"
grep -q "RADIX_EXT4_MOUNT_OK" "${qemu_log}"
grep -q "RADIX_EXT4_ROOTFS_OK" "${qemu_log}"
grep -q "RADIX_EXT4_RW_OK" "${qemu_log}"
grep -q "RADIX_EXT4_MKDIR_OK" "${qemu_log}"
grep -q "RADIX_EXT4_CREATE_OK" "${qemu_log}"
grep -q "RADIX_EXT4_RENAME_OK" "${qemu_log}"
grep -q "RADIX_EXT4_TRUNCATE_OK" "${qemu_log}"
grep -q "RADIX_EXT4_SYMLINK_OK" "${qemu_log}"
grep -q "RADIX_EXT4_UNLINK_OK" "${qemu_log}"
grep -q "RADIX_EXT4_RMDIR_OK" "${qemu_log}"
grep -q "RADIX_ROOTFS_INIT_FOUND" "${qemu_log}"
grep -q "RADIX_FAT_MOUNT_OK" "${qemu_log}"
grep -q "RADIX_FAT_RW_OK" "${qemu_log}"
grep -q "RADIX_USERMODE_ENTER_OK" "${qemu_log}"
grep -q "RADIX_USER_INIT_OK" "${qemu_log}"
grep -q "RADIX_USERMODE_EXIT_OK" "${qemu_log}"
grep -q "cmd fb" "${qemu_log}"
grep -q "cmd sched" "${qemu_log}"
if [[ "${keyboard_injected}" == "1" ]]; then
    grep -q "RADIX_IRQ_KEYBOARD_OK" "${qemu_log}"
    grep -q "RADIX_IRQ_MOUSE_OK" "${qemu_log}"
    grep -q "RADIX_IRQ_TREE_PS2_OK" "${qemu_log}"
    grep -q "RADIX_INPUT_WAKE_OK" "${qemu_log}"
    grep -q "keyboard input event" "${qemu_log}"
    grep -q "RADIX_INPUT_POINTER_EVENT_OK" "${qemu_log}"
fi

echo "x86_64 GRUB Slint smoke passed"
