# RADix-OS

RADix OS is a POSIX-inspired operating system targeting hybrid RTOS-like
performance with a POSIX-like feature set.

## Layout

- `RADixKernel/` contains the portable kernel core, public
  `<radixkernel/...>` headers, host simulator backend, and embedded/platform
  backends.
- `tools/embedded/` contains the current bootable targets and smoke scripts,
  including x86_64 GRUB terminal and RADCompositor/Slint VM profiles.
- `tests/` contains host-side RADix kernel tests.

RADLib is now an optional sibling dependency for RADLib/Slint UI integration.
Kernel-only builds do not require RADLib.

## Build Checks

Host kernel tests:

```bash
cmake -S . -B build-host -DRADIX_OS_BUILD_TESTS=ON
cmake --build build-host -j2
./build-host/tests/RADixKernelTests
```

x86_64 GRUB terminal ISO smoke:

```bash
RADIX_X86_UI_PROFILE=terminal tools/embedded/x86_64_grub_slint_smoke.sh
```

x86_64 GRUB RADCompositor/Slint ISO smoke:

```bash
RADIX_X86_UI_PROFILE=wm tools/embedded/x86_64_grub_slint_smoke.sh
```

Canonical RadBuild OS builds:

```bash
../RadBuild/radbuild/.tools/radbuild.py build os --settings settings.json --json-events
../RadBuild/radbuild/.tools/radbuild.py build os --settings settings.terminal.json --json-events
../RadBuild/radbuild/.tools/radbuild.py build os --settings settings.wm.json --json-events
```

RadBuild 0.2.1 treats each JSON as an OS build configuration. The terminal
profile excludes Slint/RADCompositor chunks and runs the terminal VM smoke; the
WM profile includes the Slint-backed RADCompositor shell, runs the hosted UI
smoke, and packages the VM image without making the current Slint VM path a
release gate.
Each profile has its own build directory, artifact directory, writable ext4/FAT
images, serial logs, and `SHA256SUMS`.

The profile ISOs are generated at:

```text
build/embedded/x86_64_grub_terminal/radixkernel-x86-64-grub-terminal.iso
build/embedded/x86_64_grub_wm/radixkernel-x86-64-grub-wm.iso
```

## API Documentation

RADix-OS uses Doxygen for the public Crimson 0.1.0 experimental kernel API
docs:

```bash
RADIX_SOURCE_DIR="$PWD" RADIX_DOCS_OUTPUT="$PWD/build/docs" doxygen docs/Doxyfile
```

The public Crimson docs are published through RadicalPackages under
`docs/radix-os/0.1.0/api/`.

## Filesystems

The x86_64 root filesystem now defaults to ext4. The current ext4 driver is a
clean no-journal profile with extent-backed read/write, create, mkdir, rename,
truncate, symlink/readlink, unlink, rmdir, chmod, and fsync support. Journal
replay and journaled metadata commits are intentionally deferred.
