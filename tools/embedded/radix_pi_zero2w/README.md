# RADix Pi Zero 2 W Standalone Payload

This target builds the RADix-owned Pi Zero 2 W kernel payload that the Circle
first-stage loader will place on the FAT boot partition as `RADIXKRN.IMG`.

The payload does not link Circle. It expects a `rad_boot_handoff_t` pointer in
`x0`; if it is booted directly as `kernel8.img`, it creates a fallback handoff
for early serial/framebuffer bring-up.

```bash
make -C tools/embedded/radix_pi_zero2w
```

Current checkpoint:

- RAD-owned BCM283x UART console
- RAD-owned system timer reads and busy sleep
- RAD-owned mailbox framebuffer registration
- RAD-owned `/dev/mmcblk0` registration scaffold
- RADix runtime boot markers and terminal commands

The eMMC command path, AArch64 EL0/process MMU path, and Slint/RADCompositor
integration are the next pieces before this target reaches x86 parity.
