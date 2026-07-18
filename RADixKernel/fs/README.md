# RADixKernel/fs — generic block-device filesystems

Board-independent filesystem drivers that operate over any registered block
device (`rad_block_device_register`). Shared by every target: e.g. the ZuBoard
A53 kernel mounts its ext4 rootfs through `x86_ext4_mount_root()` here.

- `ext4.cpp/.h` — ext4 reader/writer (mount, read/write, mkdir/create/rename/
  truncate/symlink/unlink/rmdir).
- `ext2.cpp/.h` — ext2 reader.
- `fat.cpp/.h` — FAT/vfat (boot partitions, ESP).

The public C entry points still carry an `x86_` prefix (`x86_ext4_mount_root`,
`x86_fat_mount`, …) for historical reasons; the code is arch-independent and a
later cleanup may drop the prefix.
