# RADix-OS

RADix OS is a POSIX-inspired operating system targeting hybrid RTOS-like
performance with a POSIX-like feature set.

## Layout

- `RADixKernel/` contains the portable kernel core, public
  `<radixkernel/...>` headers, host simulator backend, and embedded/platform
  backends.
- `tools/embedded/` contains the current bootable targets and smoke scripts,
  including the x86_64 GRUB + Slint VM target.
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

x86_64 GRUB + Slint ISO smoke:

```bash
tools/embedded/x86_64_grub_slint_smoke.sh
```

Canonical RadBuild OS build:

```bash
../RadBuild/radbuild/.tools/radbuild.py build os --settings settings.json --json-events
```

RadBuild 0.2.1 runs the x86_64 GRUB + Slint smoke for the configured SMP
counts, stages the ISO, kernel, ext4 rootfs, FAT32 image, serial logs, and
`SHA256SUMS` under `artifacts/radix/x86_64-grub-slint/`, and writes the
machine-readable artifact manifest under `.radmeta/artifacts/`.

The ISO is generated at:

```text
build/embedded/x86_64_grub_slint/radixkernel-x86-64-grub-slint.iso
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
