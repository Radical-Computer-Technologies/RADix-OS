#ifndef RADBOOT_H
#define RADBOOT_H

#include <radixkernel/radixkernel.h>

#ifdef __cplusplus
extern "C" {
#endif

void radboot_prepare_default(rad_boot_info_t *boot, const char *backend, const char *board, const char *console, const char *sd_mode);
void radboot_add_memory_region(rad_boot_info_t *boot, const char *name, uint64_t base, uint64_t size, uint32_t type, uint32_t flags);
void radboot_add_arg(rad_boot_info_t *boot, const char *key, const char *value);

void radboot_prepare_rp235x(rad_boot_info_t *boot, const char *board, const char *sd_mode);
void radboot_prepare_circle(rad_boot_info_t *boot, const char *board, const char *sd_mode);

#ifdef __cplusplus
}
#endif

#endif
