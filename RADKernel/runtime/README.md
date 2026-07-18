# RADKernel/runtime — shared freestanding kernel runtime

`freestanding_runtime.cpp` is the one freestanding C/C++ runtime used by the
bare-metal kernel builds (x86_64 GRUB and the aarch64 boards): a bump/free-list
heap (`malloc`/`free`/`operator new`), the mem/string family, `snprintf`, and the
C++ ABI stubs (`__cxa_*`, `abort`, `operator delete`). It replaces the old split
where x86 rolled its own runtime while a53 linked the toolchain's newlib — the
kernel now has no external libc dependency (only `-lgcc` for intrinsics).

Portable seams: spin-waits use `cpu_relax()` (pause/yield per arch); `abort()`
uses the weak `rad_hal_early_console_write` + `rad_hal_cpu_halt_forever` hooks;
C++ global-constructor running is x86-only (needs the x86 linker's `__init_array`).
