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

extern "C" void radboot_prepare_bcm283x(rad_boot_info_t *boot, const char *board, const char *sd_mode) {
    radboot_prepare_default(boot, "bcm283x_pi", board ? board : "pi-zero-2w", "/dev/console", sd_mode ? sd_mode : "auto");
    if (!boot) return;
    radboot_add_memory_region(boot, "dram", 0, 0, RAD_BOOT_MEMORY_USABLE, 0);
    radboot_add_memory_region(boot, "bcm283x-mmio", 0x3f000000u, 0x01000000u, RAD_BOOT_MEMORY_MMIO, 0);
    radboot_add_arg(boot, "bootloader", "radboot-circle-loader");
    radboot_add_arg(boot, "handoff", "rad-pi-v1");
}

extern "C" void radboot_prepare_pi_handoff(rad_boot_handoff_t *handoff, const char *payload_name, uintptr_t payload_base, uintptr_t payload_size, uintptr_t payload_entry) {
    if (!handoff) return;
    memset(handoff, 0, sizeof(*handoff));
    handoff->magic = RAD_BOOT_HANDOFF_MAGIC;
    handoff->version = RAD_BOOT_HANDOFF_VERSION;
    handoff->size = sizeof(rad_boot_handoff_t);
    handoff->kernel_image_base = payload_base;
    handoff->kernel_image_size = payload_size;
    handoff->kernel_entry = payload_entry;
    handoff->peripheral_base = 0x3f000000u;
    handoff->mailbox_base = 0x3f00b880u;
    handoff->local_interrupt_base = 0x40000000u;
    handoff->arm_memory_base = 0u;
    handoff->arm_memory_size = 512u * 1024u * 1024u;
    handoff->board_id = 0x902120u;
    handoff->entry_el = 1u;
    handoff->core_count = 4u;
    handoff->parked_core_mask = 0x0eu;
    copy_string(handoff->payload_name, sizeof(handoff->payload_name), payload_name ? payload_name : "RADKRN.IMG");
    radboot_prepare_bcm283x(&handoff->boot, "pi-zero-2w", "auto");
    handoff->boot.fdt_pointer = handoff->fdt_pointer;
    handoff->boot.initrd_pointer = handoff->initrd_pointer;
}

extern "C" rad_status_t radboot_validate_handoff(const rad_boot_handoff_t *handoff) {
    if (!handoff) return RAD_STATUS_INVALID_ARGUMENT;
    if (handoff->magic != RAD_BOOT_HANDOFF_MAGIC || handoff->version != RAD_BOOT_HANDOFF_VERSION) return RAD_STATUS_INVALID_ARGUMENT;
    if (handoff->size < sizeof(rad_boot_handoff_t)) return RAD_STATUS_INVALID_ARGUMENT;
    if (!handoff->kernel_image_base || !handoff->kernel_image_size || !handoff->kernel_entry) return RAD_STATUS_INVALID_ARGUMENT;
    if (!handoff->peripheral_base || !handoff->mailbox_base || !handoff->local_interrupt_base) return RAD_STATUS_INVALID_ARGUMENT;
    if (handoff->boot.size < sizeof(rad_boot_info_t)) return RAD_STATUS_INVALID_ARGUMENT;
    if (handoff->entry_el != 1u) return RAD_STATUS_INVALID_ARGUMENT;
    if (handoff->core_count > 1u && (handoff->parked_core_mask & 0x0eu) != 0x0eu) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t required_flags = RAD_BOOT_HANDOFF_FLAG_SECONDARIES_PARKED
        | RAD_BOOT_HANDOFF_FLAG_MMU_DISABLED
        | RAD_BOOT_HANDOFF_FLAG_DCACHE_DISABLED
        | RAD_BOOT_HANDOFF_FLAG_ICACHE_INVALIDATED
        | RAD_BOOT_HANDOFF_FLAG_TLB_INVALIDATED
        | RAD_BOOT_HANDOFF_FLAG_INTERRUPTS_MASKED;
    if ((handoff->flags & required_flags) != required_flags) return RAD_STATUS_INVALID_ARGUMENT;
    return RAD_STATUS_OK;
}
