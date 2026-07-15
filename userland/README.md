# RADix Userland

This tree owns RADix userspace program source that ships inside RADix OS images.
Target build directories under `tools/` compile and package these programs, but
they should not own the program implementations.

- `core/init`: minimal init/bootstrap binaries.
- `core/login`: login and authentication helpers.
- `core/rash`: RADix shell and shell-compatible launch shims.
- `core/radgfx`: small graphics/userland smoke helpers.
- `runtime`: freestanding userspace entry shims shared by services and tests.
