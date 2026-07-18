#ifndef RAD_X86_64_GRUB_SLINT_EXT4_H
#define RAD_X86_64_GRUB_SLINT_EXT4_H

#include <radkernel/radkernel.h>

extern "C" rad_status_t x86_ext4_mount_root(const char *block_device);
extern "C" int x86_ext4_self_test(void);

#endif
