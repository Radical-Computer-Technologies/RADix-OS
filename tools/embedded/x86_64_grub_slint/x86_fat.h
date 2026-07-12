#ifndef RAD_X86_64_GRUB_SLINT_FAT_H
#define RAD_X86_64_GRUB_SLINT_FAT_H

#include <radixkernel/radixkernel.h>

extern "C" rad_status_t x86_fat_mount(const char *block_device, const char *mount_point);
extern "C" int x86_fat_self_test(void);

#endif
