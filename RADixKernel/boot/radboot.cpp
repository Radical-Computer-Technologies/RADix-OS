#include "radboot.h"

#include <string.h>

namespace {
void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}
}

extern "C" void radboot_prepare_default(rad_boot_info_t *boot, const char *backend, const char *board, const char *console, const char *sd_mode) {
    if (!boot) return;
    memset(boot, 0, sizeof(*boot));
    boot->size = sizeof(rad_boot_info_t);
    boot->version = 1;
    copy_string(boot->backend, sizeof(boot->backend), backend);
    copy_string(boot->board, sizeof(boot->board), board);
    copy_string(boot->console_device, sizeof(boot->console_device), console);
    copy_string(boot->sd_mode, sizeof(boot->sd_mode), sd_mode);
}

extern "C" void radboot_add_memory_region(rad_boot_info_t *boot, const char *name, uint64_t base, uint64_t size, uint32_t type, uint32_t flags) {
    if (!boot || boot->memory_region_count >= RAD_BOOT_MAX_MEMORY_REGIONS) return;
    rad_boot_memory_region_t& region = boot->memory_regions[boot->memory_region_count++];
    region.base = base;
    region.size = size;
    region.type = type;
    region.flags = flags;
    copy_string(region.name, sizeof(region.name), name);
}

extern "C" void radboot_add_arg(rad_boot_info_t *boot, const char *key, const char *value) {
    if (!boot || boot->arg_count >= RAD_BOOT_MAX_ARGS) return;
    rad_boot_arg_t& arg = boot->args[boot->arg_count++];
    copy_string(arg.key, sizeof(arg.key), key);
    copy_string(arg.value, sizeof(arg.value), value);
}

extern "C" void radboot_prepare_rp235x(rad_boot_info_t *boot, const char *board, const char *sd_mode) {
    radboot_prepare_default(boot, "rp235x_pico", board ? board : "pico2", "/dev/console", sd_mode ? sd_mode : "auto");
    if (!boot) return;
    radboot_add_memory_region(boot, "sram", 0x20000000u, 520u * 1024u, RAD_BOOT_MEMORY_USABLE, 0);
    radboot_add_memory_region(boot, "xip_flash", 0x10000000u, 0, RAD_BOOT_MEMORY_RESERVED, 0);
    radboot_add_arg(boot, "bootloader", "radboot-rp235x");
}

extern "C" void radboot_prepare_circle(rad_boot_info_t *boot, const char *board, const char *sd_mode) {
    radboot_prepare_default(boot, "circle_pi", board ? board : "pi-zero-2w", "/dev/console", sd_mode ? sd_mode : "auto");
    if (!boot) return;
    radboot_add_memory_region(boot, "dram", 0, 0, RAD_BOOT_MEMORY_USABLE, 0);
    radboot_add_arg(boot, "bootloader", "radboot-circle");
}
