#ifndef RAD_X86_64_GRUB_SLINT_EXT2_H
#define RAD_X86_64_GRUB_SLINT_EXT2_H

#include <radixkernel/radixkernel.h>

extern "C" rad_status_t x86_ext2_mount_root(const char *block_device);
extern "C" int x86_ext2_self_test(void);

#endif
