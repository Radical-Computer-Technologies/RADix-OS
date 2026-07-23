# RADCompositor {#radcompositor}

RADCompositor is the Crimson 0.1.4 Slint-backed desktop service for RADPx-OS. It
is intentionally RADPx-OS-owned: Slint renders each UI surface, but RADPx-OS owns
the screen, window placement, z-order, focus, input routing, and the final
framebuffer writes.

As of 0.1.4-beta.2 the x86 VM target boots a real **multi-window desktop shell**
with a dock, an app launcher, and three kernel-rendered application windows —
Terminal, File Explorer, and Text Editor — each a live Slint surface.

## Architecture

Each window-like UI surface renders into its own off-screen pixel buffer. The
compositor (`RADCompositorCore`) tracks damaged rectangles, copies the current
front buffer forward into a software back buffer, redraws the intersecting
surfaces by z-order, and presents the touched rectangles to the framebuffer
backend through a double-buffered (front/back) present path. Surfaces may use
opaque XRGB pixels or ARGB pixels with source-over alpha blending. The
compositor holds up to 16 surfaces with a 64-entry damage ring.

The kernel-side runtime model (`EmbeddedDesktopShellModel` in `SlintShell.cpp`)
drives the shell: it owns each window record (bounds, z-order, focus, open
order), exclusive focus, and the dock/launcher state, and it maps the RADPx-OS
input queue onto the correct surface.

The x86 VM target starts these Slint surfaces:

- `RadDesktopSurface` — desktop background, top bar, and the gradient left dock
  (app launcher + open-app icons in open order, with a right-click "Close"
  dropdown per icon).
- `RadTerminalSurface` — a terminal window backed by a RADPx-OS PTY running the
  interactive shell.
- `RadFileExplorerSurface` — a real directory browser over the in-kernel VFS.
- `RadTextEditorSurface` — a text editor that opens and saves files through the
  VFS.

## Dynamic Slint in the freestanding kernel

The shell UI is **fully dynamic Slint**, not fixed-slot immediate mode. Slint
`for` repeaters over `[struct]` models and the `std-widgets` set (`Button`,
`LineEdit`, `TextEdit`, `ScrollView`) link and run in the freestanding
(`-nostdlib`, `-fno-exceptions`) kernel. This is enabled by a small set of
libstdc++ support symbols implemented in
`RADKernel/runtime/freestanding_runtime.cpp` (the `std::__throw_*` container
error handlers as diagnostic kernel panics, `std::_Sp_make_shared_tag::_S_eq`,
and `__libc_single_threaded = 1` for single-threaded shared_ptr refcounts). With
those, `std::make_shared<slint::VectorModel<T>>` works in the kernel exactly as
on the host.

Concretely:

- The File Explorer builds a `[RadFileEntry]` model from `rad_vfs_opendir` /
  `rad_vfs_readdir`, renders it with a repeater (folder/file icons, hover, a
  path breadcrumb), and descends directories via a `navigate(index)` callback.
- The Text Editor uses a `std-widgets` `TextEdit` for the document plus a
  `LineEdit` path field and Open/Save buttons, reading and writing through the
  VFS.
- The dock renders open-app icons from a `[RadDockIcon]` model in open order.

## Focus and input routing

Input events enter through the RADPx-OS input queue. Pointer coordinates are
tested against surface bounds in global screen space, translated into local
surface coordinates, and dispatched to the matching Slint window adapter; a
press on a window raises it and gives it exclusive focus.

Keyboard events route to the **focused** window's adapter. The Terminal role
translates keys to bytes and writes them to its PTY (so the interactive shell,
and escape sequences, reach the program); the other roles dispatch the key to
Slint so the Text Editor's `TextEdit` receives input. Escape no longer closes a
window — it dismisses any open menu/dropdown and otherwise forwards to the PTY.

## Client/server surfaces (userland apps)

RADCompositor also runs a Wayland-style client/server path in which an
application is a **separate userland process** that renders into a shared-memory
buffer the kernel composites — the direction the shell is moving in for the
0.1.5 line. The protocol lives on the `/dev/compositor0` device:

- A client allocates a named shm object sized to its window, `mmap`s it, and
  writes XRGB8888 pixels into that mapping. shm objects are backed by a
  physically-contiguous block so the compositor can treat the mapping as one
  linear, strided surface buffer (a full window spans many pages).
- `RAD_DEVICE_IOCTL_COMPOSITOR_CREATE_SURFACE` registers the shm buffer as a
  compositor surface; `QUEUE_DAMAGE` commits changed rectangles; `SET_BOUNDS`
  moves/resizes it; `FOCUS` raises it and requests keyboard focus;
  `DESTROY_SURFACE` tears it down.
- The kernel records the owning pid per surface and reaps a client's surfaces
  when that process exits (queuing exposed damage so the vacated area repaints).
- Input that hit-tests or focuses onto a client surface is queued to a
  per-surface ring (with surface-local pointer coordinates) and the client
  dequeues it with `POLL_INPUT` — the compositor never dispatches it into an
  in-kernel adapter.
- **The compositor owns the window frame (server-side decorations).** For each
  client surface it creates a WM-owned decoration surface just behind the
  content and draws the title/drag bar, border, and a close (X) button. The
  client renders only its content. Dragging the title bar moves the whole
  window (frame + content); clicking the close button sends the client a
  `RAD_WC_EVENT_CLOSE` event through its input queue so it can shut down
  gracefully, after which the WM reaps both surfaces. Apps therefore get
  consistent, WM-controlled decorations without drawing any chrome themselves.

`userland/lib/radcompositor` is a small freestanding C client library over this
protocol (open surface, commit, set position, focus, poll input, destroy).
`/bin/radwc-demo` is the reference client: a separate process that opens a
window surface, draws a live-animated title-barred window, moves on title-bar
drag, and cycles color on keypress. The shell auto-launches it on the x86 WM
target (`RAD_WC_DEMO_LAUNCH_OK` / `RAD_COMPOSITOR_IPC_SURFACE_OK`).
`/bin/radgfx-smoke` is a minimal one-page raw-pixel producer over the same path.

The three built-in app windows (Terminal, File Explorer, Text Editor) are still
trusted in-process Slint surfaces; porting them to userland clients over this
protocol (and building a userland Slint target) is the remaining 0.1.5 work.

## Current limits

The x86 GRUB framebuffer still uses software shadow buffers rather than a
hardware page flip, and surface buffers are currently allocated at full screen
resolution. Damage extraction is at the surface-rectangle granularity rather
than from Slint's internal dirty regions. The Pi (aarch64) target has parity
markers but does not yet link the Slint renderer; the ZuBoard is headless.
Future passes generalize shm-backed Slint applications (the client/server
model), size surface buffers per window rather than per screen, add SIMD
copy/blend backends, and make the cursor a compositor-managed surface.

## Verification

The hosted Slint self-test and the x86 VM smoke gate the compositor path with
markers for surface creation, off-screen rendering, dirty-queue submission,
copy-forward, dirty-framebuffer present, z-order, alpha blending, hit testing,
input coordinate translation, multi-window open/focus/close, and shared-memory
process IPC.
