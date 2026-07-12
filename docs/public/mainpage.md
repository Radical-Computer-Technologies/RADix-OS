# RADix-OS Kernel API

RADix-OS is a POSIX-inspired operating system kernel aimed at embedded,
desktop-VM, and future SoC targets. The kernel keeps an RTOS-influenced bias
toward explicit subsystems and deterministic control while exposing familiar
process, file, device, terminal, framebuffer, and filesystem APIs.

This documentation covers the Crimson 0.1.0 experimental kernel API.

## Current Kernel Surface

- Boot and platform handoff helpers.
- Device registry, ioctl, block, network, framebuffer, serial, input, TTY, and
  PTY APIs.
- VFS provider interface with open/read/write/seek/stat/list, directory
  mutation, symlink, truncate, chmod, and fsync hooks.
- POSIX-inspired process, file descriptor, syscall, fork, exec, wait, and
  copy-on-write interfaces.
- Module, driver, IRQ, timer, task, wait queue, mutex, and event primitives.

## Filesystem Profile

The x86_64 VM target currently boots from ext4. The implemented ext4 profile is
clean no-journal read/write ext4 with extent-backed files, allocation, create,
mkdir, rename, truncate, symlink/readlink, unlink, rmdir, chmod, and fsync.
Journal replay and journaled metadata commits are intentionally not part of
Crimson 0.1.0.

## Build And Smoke Checks

Host kernel tests:

```bash
cmake -S . -B build-host -DRADIX_OS_BUILD_TESTS=ON
cmake --build build-host -j2
./build-host/tests/RADixKernelTests
```

x86_64 GRUB + Slint VM smoke:

```bash
tools/embedded/x86_64_grub_slint_smoke.sh
```

The generated ISO is a development artifact and is not published in
RadicalPackages.
