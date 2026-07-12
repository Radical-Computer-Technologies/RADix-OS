# Pi Zero 2 W Bring-Up {#pi_zero2w}

RADix-OS now has the first pieces of the two-stage Raspberry Pi Zero 2 W port.
The intended final shape is:

- Circle remains a first-stage loader and hardware setup shim.
- Circle loads `RADIXKRN.IMG` from the FAT boot partition and jumps through
  `rad_boot_handoff_t`.
- The standalone RADix payload uses the `bcm283x_pi` backend and does not link
  Circle.

## Required Loader State

The handoff is intentionally strict for physical silicon. Before jumping to the
RADix payload, the Circle loader must:

- Park secondary cores 1-3 in a clean `wfe` loop owned by the loader handoff.
- Mask interrupts on core 0.
- Disable the MMU and data cache.
- Clean/invalidate data cache state covering the payload and handoff record.
- Invalidate the instruction cache and TLB.
- Jump to the RADix entry in AArch64 EL1.

`radboot_validate_handoff()` rejects Pi handoff records that do not declare
these conditions through the `RAD_BOOT_HANDOFF_FLAG_*` safety bits.

## Current Payload

Build the standalone payload with:

```bash
make -C tools/embedded/radix_pi_zero2w
```

The output files are:

- `tools/embedded/radix_pi_zero2w/RADIXKRN.ELF`
- `tools/embedded/radix_pi_zero2w/RADIXKRN.IMG`

The current RAD-owned backend includes:

- PL011 serial console registration.
- System timer reads and busy sleep.
- Interrupt enable/disable hooks for the BCM interrupt controller.
- Mailbox property framebuffer allocation and `/dev/fb0` registration.
- `/dev/mmcblk0` block-device registration scaffold.

## Current Limits

This page is experimental. The eMMC command path, Circle first-stage FAT load,
secondary-core park, cache/MMU teardown, jump assembly, AArch64
EL0/syscall/MMU/fork/COW path, USB input, and Slint shell parity are still
follow-up work.

QEMU can build-check the payload and may verify early serial markers when
`qemu-system-aarch64` is installed. Physical Pi Zero 2 W hardware remains the
authority for mailbox framebuffer and SD behavior.
