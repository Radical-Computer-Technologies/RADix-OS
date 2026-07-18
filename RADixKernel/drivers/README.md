# RADixKernel/drivers — portable device drivers

Device drivers that plug into the core via the ops-struct registration seams
(`rad_device_ops_t`, `rad_net_device_register`, `rad_block_device_register`,
`rad_input_device_register`). Arch/SoC-specific MMIO bring-up stays under
`platforms/`; the portable driver logic lives here.

- `net/gem_cadence.cpp` — Cadence GEM Ethernet MAC (as used on Xilinx/ZynqMP):
  BD rings, MAC loopback + SLIRP self-tests, `/dev/net0`, RX-complete IRQ.
