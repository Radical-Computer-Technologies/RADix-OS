# Changelog

All notable changes to RADPx-OS are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project aims to adhere to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Client/server compositor path (x86 VM).** A userland process can now be a
  real windowed application: it allocates a physically-contiguous shared-memory
  buffer, registers it as a compositor surface over `/dev/compositor0`, and the
  kernel composites it, routes pointer/keyboard input to it (per-surface event
  ring with surface-local coordinates), and reaps its surfaces when the process
  exits. New ioctls: `DESTROY_SURFACE`, `SET_BOUNDS`, `FOCUS`, `POLL_INPUT`.
- **Server-side window decorations.** The compositor owns each client window's
  frame: it creates a WM-owned decoration surface behind the content and draws
  the title/drag bar, border and a close (X) button. Clients render only
  content. Title-bar drag moves the whole window; the close button sends the
  client a `RAD_WC_EVENT_CLOSE` event so it can shut down (the WM then reaps
  both surfaces). New markers: `RAD_COMPOSITOR_DECORATION_OK`,
  `RAD_COMPOSITOR_DECORATION_CLOSE_OK`.
- `userland/lib/radcompositor`: a freestanding C client library for the surface
  protocol. `/bin/radwc-demo`: the reference interactive client (draggable,
  color-cycles on keypress), auto-launched on the WM target and gated in the
  x86 WM smoke (`RAD_WC_DEMO_LAUNCH_OK`, `RAD_COMPOSITOR_IPC_SURFACE_OK`).
- Contiguous multi-page shm backing (`x86_vm_alloc_pages`); `RAD_SHM_MAX_PAGES`
  16 -> 1024 (4 MiB/surface), `RAD_KERNEL_MAX_SHM_OBJECTS` 8 -> 16.

### Known issues

- A fully-blocking `nanosleep` in a userland task is not re-woken on a worker
  core; compositor clients pace cooperatively (spin + yield) for now.

## [0.1.4-beta.2] - 2026-07-22

### Added

- **Multi-window RADCompositor desktop shell (x86 VM).** The Slint window manager
  now hosts three independently movable/resizable/closable app windows with exclusive
  focus and z-order: a **Terminal** (radsh over a PTY), a **File Explorer**, and a
  **Text Editor**.
- **File Explorer** with a real clickable listing (folder/file icons, hover, a path
  breadcrumb) that navigates directories via the in-kernel VFS.
- **Text Editor** window: a Slint `TextEdit` with an Open/Save toolbar backed by the
  kernel VFS.
- **Dynamic Slint UIs in the freestanding kernel.** `for`-Repeaters over `[struct]`
  models and Slint `std-widgets` (`Button`/`LineEdit`/`TextEdit`/`ScrollView`) now
  link and run under `-nostdlib -fno-exceptions` by providing the required libstdc++
  support symbols (container `__throw_*` handlers as reported kernel panics,
  `_Sp_make_shared_tag::_S_eq`, `__libc_single_threaded`).
- **Gradient dock** with open-order app icons (first-opened at the top), a per-icon
  right-click "Close" menu, and window geometry that resets to defaults on reopen.
- `vim` (vim-tiny) included in the x86 RADCompositor image.

### Changed

- 2026-07-18: Rebranded to **RADPx-OS** (Radical Posix OS) with the kernel named
  **RADKernel**. Renamed the code namespace `radix` -> `rad` and `RADixKernel`
  -> `RADKernel`. The public `rad_` / `RAD_` API prefixes are unchanged.
- Simplified the compositor to a provably-correct baseline (always copy-forward,
  full-footprint damage); `-enable-kvm` makes the removed micro-optimizations moot.

### Fixed

- Terminal no longer blanks to black (the kernel transcript feed no longer overwrites
  live radsh output) and no longer freezes on a command-not-found (execve pre-flights
  the target and returns -1 so the shell prints "command not found" and exits 127).
- Terminal keyboard input restored under the multi-window key routing (keys route to
  the focused window; only the terminal forwards to the PTY).
- Dragging a window over another no longer corrupts the overlapped window's rendering.

### Embedded

- a53/Pi brought to x86 interrupt/SMP-safety parity: Pi FP/NEON save across interrupt
  entry and an a53 page-allocator lock.
