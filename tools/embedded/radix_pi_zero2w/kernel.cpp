#include <radixkernel/radixkernel.h>
#include <radboot.h>

#include <stdint.h>
#include <string.h>

extern "C" char __bss_start;
extern "C" char __bss_end;
extern "C" char __image_end;
extern "C" void rad_bcm283x_bind_handoff(const rad_boot_handoff_t *handoff);

namespace {
void clear_bss() {
    memset(&__bss_start, 0, static_cast<size_t>(&__bss_end - &__bss_start));
}

void marker(const char *text) {
    rad_debug_marker(text);
}

void write_terminal(const char *text, void *context) {
    (void)context;
    rad_device_t console = nullptr;
    if (rad_device_open("/dev/console", &console) != RAD_STATUS_OK) return;
    size_t written = 0;
    rad_device_write(console, text, strlen(text), &written);
    rad_device_close(console);
}
}

extern "C" void radix_pi_entry(rad_boot_handoff_t *handoff) {
    clear_bss();

    rad_boot_handoff_t fallback{};
    if (radboot_validate_handoff(handoff) != RAD_STATUS_OK) {
        radboot_prepare_pi_handoff(&fallback, "direct-kernel8", 0x80000u, reinterpret_cast<uintptr_t>(&__image_end) - 0x80000u, reinterpret_cast<uintptr_t>(&radix_pi_entry));
        fallback.flags = RAD_BOOT_HANDOFF_FLAG_INTERRUPTS_MASKED;
        fallback.parked_core_mask = 0u;
        handoff = &fallback;
    }

    rad_bcm283x_bind_handoff(handoff);

    rad_kernel_config_t config{};
    config.backend_name = "bcm283x_pi";
    config.boot_info = &handoff->boot;
    rad_kernel_init(&config);

    marker("RADIX_PI_HANDOFF_OK");
    marker("RADIX_PI_PAYLOAD_ENTRY_OK");
    if ((handoff->flags & RAD_BOOT_HANDOFF_FLAG_SECONDARIES_PARKED) != 0) marker("RADIX_PI_SECONDARIES_PARKED_OK");
    if ((handoff->flags & RAD_BOOT_HANDOFF_FLAG_MMU_DISABLED) != 0
        && (handoff->flags & RAD_BOOT_HANDOFF_FLAG_DCACHE_DISABLED) != 0
        && (handoff->flags & RAD_BOOT_HANDOFF_FLAG_ICACHE_INVALIDATED) != 0
        && (handoff->flags & RAD_BOOT_HANDOFF_FLAG_TLB_INVALIDATED) != 0) {
        marker("RADIX_PI_CLEAN_CPU_STATE_OK");
    }

    rad_terminal_execute("bootinfo", write_terminal, nullptr);
    rad_terminal_execute("cores", write_terminal, nullptr);
    rad_terminal_execute("devices", write_terminal, nullptr);
    rad_terminal_execute("fb", write_terminal, nullptr);

    rad_framebuffer_t framebuffer = nullptr;
    if (rad_framebuffer_open_primary(&framebuffer) == RAD_STATUS_OK) {
        rad_framebuffer_clear(framebuffer, 0x00071422u);
        rad_framebuffer_draw_text(framebuffer, 2, 2, "RADix Pi Zero 2 W", 0x00f8fafcu, 0x00071422u);
        rad_framebuffer_draw_text(framebuffer, 2, 4, "RAD-owned bcm283x runtime", 0x00d1fae5u, 0x00071422u);
        rad_framebuffer_rect_t rect{0, 0, 1280, 720};
        rad_framebuffer_flush(framebuffer, &rect);
        marker("RADIX_PI_FRAMEBUFFER_TEXT_OK");
    }

    while (!rad_kernel_is_shutdown_requested()) {
        rad_kernel_poll();
        rad_sleep_ms(1);
    }

    rad_kernel_shutdown();
    for (;;) asm volatile("wfe");
}
