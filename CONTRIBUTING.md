# Contributing to RADPx-OS

RADPx-OS (Radical Posix OS) is a POSIX-inspired operating system targeting
hybrid RTOS-like performance. The kernel is **RADKernel**. This guide covers the
actual build, test, and smoke workflow for the repo.

## Repository layout

- `RADKernel/` — the portable kernel core, public `<radkernel/...>` headers, the
  host simulator backend, and embedded/platform backends (x86_64, and the
  Cortex-A53 `a53/` tree with the ZynqMP/ZuBoard SoC backend).
- `tools/embedded/` — bootable targets and smoke scripts: the x86_64 GRUB
  terminal and RADCompositor/Slint VM profiles, and the ZuBoard-1CG (ZynqMP A53)
  serial target under `rad_zuboard_1cg/`.
- `tests/` — host-side RADPx-OS kernel tests.
- `docs/` — Doxygen config (`docs/Doxyfile`) and public docs under `docs/public/`.

RADLib is an optional sibling dependency, used only for the RADLib/Slint UI
integration. Kernel-only builds do not require it.

## Building with RadBuild

RadBuild is the build driver. Install the frozen release once, then use the
`radbuild` command directly:

```bash
sudo apt install ./radbuild_0.2.1_amd64.deb
```

Each `settings*.json` at the repo root is an OS build configuration. RadBuild
resolves it, builds the image, and runs the target's smoke:

```bash
radbuild build os --settings settings.json          --json-events   # default x86_64 terminal
radbuild build os --settings settings.terminal.json --json-events   # x86_64 GRUB terminal
radbuild build os --settings settings.wm.json       --json-events   # x86_64 RADCompositor/Slint WM
radbuild build os --settings settings.ci.json --system rad-zuboard-1cg-qemu-ci --json-events  # ZuBoard-1CG A53 (QEMU)
```

Each profile has its own build directory, artifact directory, writable ext4/FAT
images, serial logs, and `SHA256SUMS`. Development against an unreleased RadBuild
can still run the source tree directly
(`python3 ../RadBuild/radbuild/.tools/radbuild.py ...`), but the frozen
`radbuild` command is the supported entry point.

## Build checks and smokes

Host kernel tests:

```bash
cmake -S . -B build-host -DRADPX_OS_BUILD_TESTS=ON
cmake --build build-host -j2
./build-host/tests/RADKernelTests
```

x86_64 GRUB ISO smokes (select the UI profile with `RAD_X86_UI_PROFILE`):

```bash
RAD_X86_UI_PROFILE=terminal tools/embedded/x86_64_grub_slint_smoke.sh
RAD_X86_UI_PROFILE=wm       tools/embedded/x86_64_grub_slint_smoke.sh
```

ZuBoard-1CG (ZynqMP A53) QEMU smokes — the marker gate boots to an interactive
serial login, the login smoke drives that session, and the SMP smoke validates
the second core under `-smp 2`:

```bash
tools/embedded/rad_zuboard_1cg/qemu_marker_smoke.sh   # ordered marker gate
tools/embedded/rad_zuboard_1cg/qemu_login_smoke.sh    # interactive login/ls/cat/ps
tools/embedded/rad_zuboard_1cg/qemu_smp_smoke.sh      # second A53 core (-smp 2)
```

A change should keep the host tests and the relevant target smoke green before it
lands.

## Coding style and conventions

- The public kernel API lives under `<radkernel/...>` and uses the `rad_` /
  `RAD_` code prefixes. Keep these prefixes when adding public symbols.
- Kernel modules use the `.rko` extension; future RADPx dynamic shared objects
  use `.rso`.
- The x86_64 and ZuBoard A53 root filesystems default to a clean no-journal ext4
  profile; journal replay and journaled metadata commits are intentionally
  deferred, so do not assume them in new code.

## Branch and commit conventions

- Work on a topic branch (current line: `crimson-v0.1.4`); do not commit directly
  to `main`.
- Keep commits small, incremental, and green — each commit should build and pass
  the relevant tests/smoke.
- Commits are authored as the maintainer.

## Documentation

RADPx-OS uses Doxygen for the public Crimson experimental kernel API docs:

```bash
RAD_SOURCE_DIR="$PWD" RAD_DOCS_OUTPUT="$PWD/build/docs" doxygen docs/Doxyfile
```

Narrative docs live under `docs/public/`. The public Crimson docs are published
through RadicalPackages under `docs/radpx-os/<version>/api/`.
