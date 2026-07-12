#include <radboot.h>
#include <radixkernel/radixkernel.h>

#include "x86_cpu.h"
#include "x86_ext2.h"
#include "x86_ext4.h"
#include "x86_fat.h"
#include "x86_storage.h"
#include "x86_user.h"
#include "x86_vm.h"
#include "slint_shell.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_init(void);
extern "C" void x86_serial_write(const char *text);
extern "C" char x86_boot_stack_top[];
extern "C" int x86_cpp_runtime_self_test(void);
extern "C" int snprintf(char *buffer, size_t size, const char *format, ...);

namespace {
constexpr uint32_t Multiboot2BootloaderMagic = 0x36d76289u;
constexpr uint32_t TagEnd = 0u;
constexpr uint32_t TagMmap = 6u;
constexpr uint32_t TagFramebuffer = 8u;
constexpr uint32_t TagAcpiOld = 14u;
constexpr uint32_t TagAcpiNew = 15u;

struct Mb2Tag {
    uint32_t type;
    uint32_t size;
};

struct Mb2MmapEntry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct Mb2MmapTag {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    Mb2MmapEntry entries[];
};

struct Mb2FramebufferTag {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t kind;
    uint16_t reserved;
};

struct [[gnu::packed]] AcpiRsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
};

struct [[gnu::packed]] AcpiSdtHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

struct [[gnu::packed]] AcpiMadt {
    AcpiSdtHeader header;
    uint32_t local_apic_address;
    uint32_t flags;
    uint8_t entries[];
};

struct CpuTopologyProbe {
    uint32_t detected_cores;
    uint32_t present_mask;
    uint64_t lapic_address;
};

char g_transcript[8192];
size_t g_transcript_size = 0;
int g_keyboard_input_seen = 0;
int g_pointer_input_seen = 0;
int g_module_probe_inits = 0;
int g_module_probe_exits = 0;
int g_deferred_work_ran = 0;
int g_scheduler_probe_ran = 0;
int g_ap_scheduler_probe_core = -1;
volatile int g_preempt_busy_done = 0;
volatile int g_preempt_observer_ran = 0;

void append_text(const char *text) {
    if (!text) return;
    while (*text && g_transcript_size + 1 < sizeof(g_transcript)) {
        g_transcript[g_transcript_size++] = *text++;
    }
    g_transcript[g_transcript_size] = '\0';
}

void append_bytes(const char *text, size_t size) {
    for (size_t i = 0; i < size && g_transcript_size + 1 < sizeof(g_transcript); ++i) {
        g_transcript[g_transcript_size++] = text[i];
    }
    g_transcript[g_transcript_size] = '\0';
}

void serial_hex64(uint64_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    char out[19];
    out[0] = '0';
    out[1] = 'x';
    for (int i = 0; i < 16; ++i) out[2 + i] = digits[(value >> ((15 - i) * 4)) & 0xf];
    out[18] = '\0';
    x86_serial_write(out);
}

const Mb2Tag *first_tag(uint32_t info_addr) {
    const auto *base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(info_addr));
    return reinterpret_cast<const Mb2Tag*>(base + 8);
}

const Mb2Tag *next_tag(const Mb2Tag *tag) {
    return reinterpret_cast<const Mb2Tag*>(reinterpret_cast<const uint8_t*>(tag) + ((tag->size + 7u) & ~uint32_t(7u)));
}

const Mb2FramebufferTag *find_framebuffer(uint32_t info_addr) {
    const auto *base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(info_addr));
    const uint32_t total_size = *reinterpret_cast<const uint32_t*>(base);
    const auto *tag = first_tag(info_addr);
    const auto *end = reinterpret_cast<const Mb2Tag*>(base + total_size);
    while (tag < end && tag->type != TagEnd) {
        if (tag->type == TagFramebuffer && tag->size >= sizeof(Mb2FramebufferTag)) {
            return reinterpret_cast<const Mb2FramebufferTag*>(tag);
        }
        tag = next_tag(tag);
    }
    return nullptr;
}

int bytes_equal(const char *a, const char *b, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

int acpi_checksum_ok(const void *data, size_t size) {
    if (!data || !size) return 0;
    const auto *bytes = static_cast<const uint8_t*>(data);
    uint8_t sum = 0;
    for (size_t i = 0; i < size; ++i) sum = static_cast<uint8_t>(sum + bytes[i]);
    return sum == 0;
}

const AcpiRsdp *find_acpi_rsdp(uint32_t info_addr) {
    const auto *base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(info_addr));
    const uint32_t total_size = *reinterpret_cast<const uint32_t*>(base);
    const auto *tag = first_tag(info_addr);
    const auto *end = reinterpret_cast<const Mb2Tag*>(base + total_size);
    while (tag < end && tag->type != TagEnd) {
        if ((tag->type == TagAcpiOld || tag->type == TagAcpiNew)
            && tag->size >= sizeof(Mb2Tag) + 20u) {
            const auto *rsdp = reinterpret_cast<const AcpiRsdp*>(
                reinterpret_cast<const uint8_t*>(tag) + sizeof(Mb2Tag));
            if (bytes_equal(rsdp->signature, "RSD PTR ", 8)
                && acpi_checksum_ok(rsdp, 20u)) {
                if (rsdp->revision >= 2 && rsdp->length >= sizeof(AcpiRsdp)
                    && !acpi_checksum_ok(rsdp, rsdp->length)) {
                    tag = next_tag(tag);
                    continue;
                }
                return rsdp;
            }
        }
        tag = next_tag(tag);
    }
    return nullptr;
}

const AcpiSdtHeader *find_acpi_table(const AcpiRsdp *rsdp, const char signature[4]) {
    if (!rsdp) return nullptr;
    if (rsdp->revision >= 2 && rsdp->xsdt_address) {
        const auto *xsdt = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uintptr_t>(rsdp->xsdt_address));
        if (bytes_equal(xsdt->signature, "XSDT", 4) && acpi_checksum_ok(xsdt, xsdt->length)) {
            const size_t entries = (xsdt->length - sizeof(AcpiSdtHeader)) / sizeof(uint64_t);
            const auto *entry = reinterpret_cast<const uint64_t*>(
                reinterpret_cast<const uint8_t*>(xsdt) + sizeof(AcpiSdtHeader));
            for (size_t i = 0; i < entries; ++i) {
                const auto *table = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uintptr_t>(entry[i]));
                if (table && bytes_equal(table->signature, signature, 4) && acpi_checksum_ok(table, table->length)) {
                    return table;
                }
            }
        }
    }
    if (rsdp->rsdt_address) {
        const auto *rsdt = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uintptr_t>(rsdp->rsdt_address));
        if (bytes_equal(rsdt->signature, "RSDT", 4) && acpi_checksum_ok(rsdt, rsdt->length)) {
            const size_t entries = (rsdt->length - sizeof(AcpiSdtHeader)) / sizeof(uint32_t);
            const auto *entry = reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const uint8_t*>(rsdt) + sizeof(AcpiSdtHeader));
            for (size_t i = 0; i < entries; ++i) {
                const auto *table = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uintptr_t>(entry[i]));
                if (table && bytes_equal(table->signature, signature, 4) && acpi_checksum_ok(table, table->length)) {
                    return table;
                }
            }
        }
    }
    return nullptr;
}

CpuTopologyProbe detect_cpu_topology(uint32_t info_addr) {
    CpuTopologyProbe topology{1u, 1u, 0xfee00000ull};
    const AcpiRsdp *rsdp = find_acpi_rsdp(info_addr);
    const auto *madt_header = find_acpi_table(rsdp, "APIC");
    if (!madt_header || madt_header->length < sizeof(AcpiMadt)) return topology;
    const auto *madt = reinterpret_cast<const AcpiMadt*>(madt_header);
    topology.lapic_address = madt->local_apic_address ? madt->local_apic_address : topology.lapic_address;
    topology.detected_cores = 0;
    topology.present_mask = 0;
    const uint8_t *entry = madt->entries;
    const uint8_t *end = reinterpret_cast<const uint8_t*>(madt) + madt->header.length;
    while (entry + 2 <= end) {
        const uint8_t type = entry[0];
        const uint8_t length = entry[1];
        if (length < 2 || entry + length > end) break;
        uint32_t enabled = 0;
        if (type == 0 && length >= 8) {
            const uint32_t flags = *reinterpret_cast<const uint32_t*>(entry + 4);
            enabled = flags & 1u;
        } else if (type == 9 && length >= 16) {
            const uint32_t flags = *reinterpret_cast<const uint32_t*>(entry + 8);
            enabled = flags & 1u;
        } else if (type == 5 && length >= 12) {
            topology.lapic_address = *reinterpret_cast<const uint64_t*>(entry + 4);
        }
        if (enabled) {
            if (topology.detected_cores < 32u) topology.present_mask |= (1u << topology.detected_cores);
            ++topology.detected_cores;
        }
        entry += length;
    }
    if (topology.detected_cores == 0) {
        topology.detected_cores = 1;
        topology.present_mask = 1;
    }
    return topology;
}

void add_memory_regions(rad_boot_info_t *boot, uint32_t info_addr) {
    const auto *base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(info_addr));
    const uint32_t total_size = *reinterpret_cast<const uint32_t*>(base);
    const auto *tag = first_tag(info_addr);
    const auto *end = reinterpret_cast<const Mb2Tag*>(base + total_size);
    while (tag < end && tag->type != TagEnd) {
        if (tag->type == TagMmap && tag->size >= sizeof(Mb2MmapTag)) {
            const auto *mmap = reinterpret_cast<const Mb2MmapTag*>(tag);
            const auto *entry = mmap->entries;
            const auto *entry_end = reinterpret_cast<const Mb2MmapEntry*>(
                reinterpret_cast<const uint8_t*>(tag) + tag->size);
            while (entry < entry_end && boot->memory_region_count < RAD_BOOT_MAX_MEMORY_REGIONS) {
                radboot_add_memory_region(boot,
                    entry->type == 1 ? "usable" : "reserved",
                    entry->addr,
                    entry->len,
                    entry->type == 1 ? RAD_BOOT_MEMORY_USABLE : RAD_BOOT_MEMORY_RESERVED,
                    0);
                entry = reinterpret_cast<const Mb2MmapEntry*>(
                    reinterpret_cast<const uint8_t*>(entry) + mmap->entry_size);
            }
        }
        tag = next_tag(tag);
    }
}

rad_status_t register_framebuffer(const Mb2FramebufferTag *fb) {
    if (!fb || fb->addr == 0 || fb->bpp != 32) return RAD_STATUS_NOT_SUPPORTED;
    rad_framebuffer_config_t config{};
    config.size = sizeof(config);
    config.name = "/dev/fb0";
    config.info.size = sizeof(config.info);
    config.info.width = fb->width;
    config.info.height = fb->height;
    config.info.stride_bytes = fb->pitch;
    config.info.pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
    config.info.pixels = reinterpret_cast<void*>(static_cast<uintptr_t>(fb->addr));
    config.output_type = RAD_DISPLAY_OUTPUT_GRUB;
    config.connector = "multiboot2-framebuffer";
    config.mode_count = 1;
    config.preferred_mode = 0;
    config.modes[0].width = fb->width;
    config.modes[0].height = fb->height;
    config.modes[0].refresh_hz = 60;
    config.modes[0].stride_bytes = fb->pitch;
    config.modes[0].pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
    config.primary = 1;
    rad_framebuffer_ops_t ops{};
    return rad_framebuffer_register_ex(&config, &ops);
}

void render_terminal(rad_framebuffer_t framebuffer) {
    rad_framebuffer_info_t info{};
    if (rad_framebuffer_get_info(framebuffer, &info) != RAD_STATUS_OK) return;
    rad_framebuffer_clear(framebuffer, 0x00071422u);
    rad_framebuffer_draw_text(framebuffer, 1, 1, "RADKernel x86_64 VM Terminal", 0x00e5f3ffu, 0x0010273au);
    rad_framebuffer_draw_text(framebuffer, 1, 3, "Slint target: freestanding runtime integration pending; PTY shell active", 0x00fef3c7u, 0x00071422u);

    const uint32_t rows = info.height / 8u;
    const uint32_t cols = info.width / 8u;
    uint32_t y = 5;
    char line[160];
    size_t pos = 0;
    for (size_t i = 0; i <= g_transcript_size && y + 1 < rows; ++i) {
        const char ch = g_transcript[i];
        if (ch != '\n' && ch != '\0' && pos + 1 < sizeof(line) && pos + 1 < cols) {
            line[pos++] = ch;
            continue;
        }
        line[pos] = '\0';
        rad_framebuffer_draw_text(framebuffer, 1, y++, line, 0x00d1fae5u, 0x00020617u);
        pos = 0;
    }
    rad_framebuffer_rect_t rect{0, 0, info.width, info.height};
    rad_framebuffer_flush(framebuffer, &rect);
}

void pty_drain(rad_pty_t pty) {
    char buffer[512];
    size_t count = 0;
    while (rad_pty_read_master(pty, buffer, sizeof(buffer), &count) == RAD_STATUS_OK && count > 0) {
        append_bytes(buffer, count);
    }
}

void pty_command(rad_pty_t pty, rad_tty_t slave, const char *command) {
    size_t ignored = 0;
    append_text("$ ");
    append_text(command);
    append_text("\n");
    rad_pty_write_master(pty, command, strlen(command), &ignored);
    rad_pty_write_master(pty, "\n", 1, &ignored);
    rad_terminal_poll_tty(slave);
    pty_drain(pty);
}

void posix_abi_self_test(void) {
    rad_posix_timeval_t tv{};
    const intptr_t pid = rad_syscall_dispatch(RAD_SYSCALL_GETPID, 0, 0, 0, 0, 0, 0);
    const intptr_t time_status = rad_syscall_dispatch(RAD_SYSCALL_GETTIMEOFDAY, reinterpret_cast<uintptr_t>(&tv), 0, 0, 0, 0, 0);
    const char message[] = "RADix POSIX ABI stdio probe\n";
    const intptr_t wrote = rad_syscall_dispatch(RAD_SYSCALL_WRITE, 1, reinterpret_cast<uintptr_t>(message), sizeof(message) - 1, 0, 0, 0);
    if (pid == 1 && time_status == RAD_STATUS_OK && wrote == static_cast<intptr_t>(sizeof(message) - 1)) {
        append_text("RADIX_POSIX_ABI_OK\n");
        rad_debug_marker("RADIX_POSIX_ABI_OK");
    } else {
        append_text("RADIX_POSIX_ABI_FAIL\n");
        rad_debug_marker("RADIX_POSIX_ABI_FAIL");
    }
}

rad_status_t module_probe_init(void*) {
    ++g_module_probe_inits;
    return RAD_STATUS_OK;
}

void module_probe_exit(void*) {
    ++g_module_probe_exits;
}

void module_lifecycle_self_test(void) {
    rad_module_descriptor_t descriptor{};
    descriptor.size = sizeof(descriptor);
    descriptor.name = "rad_x86_cpp_probe";
    descriptor.bus = "x86";
    descriptor.compatible = "rad,x86-cpp-probe";
    descriptor.init = module_probe_init;
    descriptor.exit = module_probe_exit;
    rad_module_info_t modules[4]{};
    const rad_status_t registered = rad_module_register(&descriptor);
    const size_t count = rad_module_list(modules, 4);
    int listed = 0;
    for (size_t i = 0; i < count && i < 4; ++i) {
        if (strcmp(modules[i].name, "rad_x86_cpp_probe") == 0
            && modules[i].state == RAD_MODULE_INITIALIZED
            && modules[i].last_status == RAD_STATUS_OK) {
            listed = 1;
        }
    }
    const rad_status_t unregistered = rad_module_unregister("rad_x86_cpp_probe");
    if (registered == RAD_STATUS_OK && unregistered == RAD_STATUS_OK && listed
        && g_module_probe_inits == 1 && g_module_probe_exits == 1) {
        append_text("RADIX_MODULE_LIFECYCLE_OK\n");
        rad_debug_marker("RADIX_MODULE_LIFECYCLE_OK");
    } else {
        append_text("RADIX_MODULE_LIFECYCLE_FAIL\n");
        rad_debug_marker("RADIX_MODULE_LIFECYCLE_FAIL");
    }
}

void deferred_work_self_test_handler(void*) {
    ++g_deferred_work_ran;
}

void scheduler_probe_task(void*) {
    ++g_scheduler_probe_ran;
}

void ap_scheduler_probe_task(void*) {
    g_ap_scheduler_probe_core = rad_task_current_core();
}

void preempt_busy_task(void*) {
    volatile uint64_t spin = 0;
    while (!g_preempt_observer_ran && spin < 1000000ull) {
        ++spin;
    }
    g_preempt_busy_done = g_preempt_observer_ran ? 1 : -1;
}

void preempt_observer_task(void*) {
    g_preempt_observer_ran = 1;
}

void runtime_self_test(void) {
    size_t ran = 0;
    const rad_status_t submit = rad_work_submit("boot-self-test", deferred_work_self_test_handler, nullptr);
    const rad_status_t poll = rad_work_poll(8, &ran);
    if (submit == RAD_STATUS_OK && poll == RAD_STATUS_OK && ran > 0 && g_deferred_work_ran == 1) {
        append_text("RADIX_DEFERRED_WORK_OK\n");
        rad_printk("RADIX_DEFERRED_WORK_OK\n");
    } else {
        append_text("RADIX_DEFERRED_WORK_FAIL\n");
        rad_printk("RADIX_DEFERRED_WORK_FAIL\n");
    }

    rad_wait_queue_t queue = nullptr;
    const rad_status_t created = rad_wait_queue_create(&queue);
    const rad_status_t wake = created == RAD_STATUS_OK ? rad_wait_queue_wake_one(queue) : RAD_STATUS_ERROR;
    const rad_status_t wait = created == RAD_STATUS_OK ? rad_wait_queue_wait(queue, 1) : RAD_STATUS_ERROR;
    if (queue) rad_wait_queue_destroy(queue);
    if (created == RAD_STATUS_OK && wake == RAD_STATUS_OK && wait == RAD_STATUS_OK) {
        append_text("RADIX_WAIT_QUEUE_OK\n");
        rad_printk("RADIX_WAIT_QUEUE_OK\n");
    } else {
        append_text("RADIX_WAIT_QUEUE_FAIL\n");
        rad_printk("RADIX_WAIT_QUEUE_FAIL\n");
    }

    rad_scheduler_info_t scheduler{};
    if (rad_scheduler_info_get(&scheduler) == RAD_STATUS_OK
        && scheduler.detected_cores >= 1
        && scheduler.arch[0] != '\0') {
        append_text("RADIX_SCHED_CORE_OK\n");
        rad_printk("RADIX_SCHED_CORE_OK\n");
    } else {
        append_text("RADIX_SCHED_CORE_FAIL\n");
        rad_printk("RADIX_SCHED_CORE_FAIL\n");
    }

    const uint64_t ticks_before = scheduler.scheduler_ticks;
    rad_task_t busy_task = nullptr;
    rad_task_t observer_task = nullptr;
    rad_task_config_t busy_config{};
    busy_config.size = sizeof(busy_config);
    busy_config.name = "preempt-busy";
    busy_config.target_core = RAD_TASK_CORE_SERVICE;
    rad_task_config_t observer_config{};
    observer_config.size = sizeof(observer_config);
    observer_config.name = "preempt-observer";
    observer_config.target_core = RAD_TASK_CORE_SERVICE;
    const rad_status_t busy_created = rad_task_create_config(&busy_task, &busy_config, preempt_busy_task, nullptr);
    const rad_status_t observer_created = rad_task_create_config(&observer_task, &observer_config, preempt_observer_task, nullptr);
    if (busy_created == RAD_STATUS_OK
        && observer_created == RAD_STATUS_OK
        && rad_task_join(observer_task) == RAD_STATUS_OK
        && rad_task_join(busy_task) == RAD_STATUS_OK
        && rad_scheduler_info_get(&scheduler) == RAD_STATUS_OK
        && scheduler.preemption_supported
        && scheduler.preemption_enabled
        && scheduler.scheduler_ticks > ticks_before
        && g_preempt_observer_ran == 1
        && g_preempt_busy_done == 1) {
        append_text("RADIX_PREEMPT_SCHED_OK\n");
        rad_printk("RADIX_PREEMPT_SCHED_OK\n");
    } else {
        append_text("RADIX_PREEMPT_SCHED_FAIL\n");
        rad_printk("RADIX_PREEMPT_SCHED_FAIL\n");
    }

    if (scheduler.detected_cores > 1 && (scheduler.worker_running_mask & 0x2u)) {
        rad_task_t ap_task = nullptr;
        rad_task_config_t ap_task_config{};
        ap_task_config.size = sizeof(ap_task_config);
        ap_task_config.name = "ap-scheduler-probe";
        ap_task_config.target_core = 1;
        if (rad_task_create_config(&ap_task, &ap_task_config, ap_scheduler_probe_task, nullptr) == RAD_STATUS_OK
            && rad_task_join(ap_task) == RAD_STATUS_OK
            && g_ap_scheduler_probe_core == 1) {
            append_text("RADIX_AP_WORKER_TASK_OK\n");
            rad_printk("RADIX_AP_WORKER_TASK_OK\n");
        } else {
            append_text("RADIX_AP_WORKER_TASK_FAIL\n");
            rad_printk("RADIX_AP_WORKER_TASK_FAIL\n");
        }
    }

    const uint64_t switches_before = scheduler.context_switches;
    rad_task_t scheduler_task = nullptr;
    rad_task_config_t scheduler_task_config{};
    scheduler_task_config.size = sizeof(scheduler_task_config);
    scheduler_task_config.name = "scheduler-probe";
    scheduler_task_config.target_core = RAD_TASK_CORE_SERVICE;
    if (rad_task_create_config(&scheduler_task, &scheduler_task_config, scheduler_probe_task, nullptr) == RAD_STATUS_OK
        && rad_task_join(scheduler_task) == RAD_STATUS_OK
        && rad_scheduler_info_get(&scheduler) == RAD_STATUS_OK
        && g_scheduler_probe_ran == 1
        && scheduler.context_switches > switches_before) {
        append_text("RADIX_CONTEXT_DISPATCH_OK\n");
        rad_printk("RADIX_CONTEXT_DISPATCH_OK\n");
    } else {
        append_text("RADIX_CONTEXT_DISPATCH_FAIL\n");
        rad_printk("RADIX_CONTEXT_DISPATCH_FAIL\n");
    }
}

void pump_serial_to_pty(rad_device_t serial, rad_pty_t pty, rad_tty_t slave) {
    char ch = 0;
    size_t done = 0;
    while (rad_device_read(serial, &ch, 1, &done) == RAD_STATUS_OK && done == 1) {
        size_t ignored = 0;
        rad_pty_write_master(pty, &ch, 1, &ignored);
        rad_terminal_poll_tty(slave);
        pty_drain(pty);
    }
}

void send_terminal_bytes(rad_pty_t pty, rad_tty_t slave, const char *bytes, size_t size) {
    if (!bytes || !size) return;
    size_t ignored = 0;
    rad_pty_write_master(pty, bytes, size, &ignored);
    rad_terminal_poll_tty(slave);
    pty_drain(pty);
}

void input_event_to_pty(const rad_input_event_t *event, rad_pty_t pty, rad_tty_t slave) {
    if (!event || event->type != RAD_INPUT_EVENT_KEY || !event->pressed) return;
    if (!g_keyboard_input_seen) {
        g_keyboard_input_seen = 1;
        x86_serial_write("keyboard input event\n");
    }

    if (event->codepoint) {
        char ch = static_cast<char>(event->codepoint);
        if (event->modifiers & RAD_INPUT_MOD_CTRL) {
            if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
            if (ch >= 'a' && ch <= 'z') {
                ch = static_cast<char>(ch - 'a' + 1);
            }
        }
        send_terminal_bytes(pty, slave, &ch, 1);
        return;
    }

    switch (event->code) {
    case RAD_INPUT_KEY_ENTER:
        send_terminal_bytes(pty, slave, "\n", 1);
        break;
    case RAD_INPUT_KEY_BACKSPACE:
        send_terminal_bytes(pty, slave, "\x7f", 1);
        break;
    case RAD_INPUT_KEY_TAB:
        send_terminal_bytes(pty, slave, "\t", 1);
        break;
    case RAD_INPUT_KEY_ESCAPE:
        send_terminal_bytes(pty, slave, "\x1b", 1);
        break;
    case RAD_INPUT_KEY_UP:
        send_terminal_bytes(pty, slave, "\x1b[A", 3);
        break;
    case RAD_INPUT_KEY_DOWN:
        send_terminal_bytes(pty, slave, "\x1b[B", 3);
        break;
    case RAD_INPUT_KEY_RIGHT:
        send_terminal_bytes(pty, slave, "\x1b[C", 3);
        break;
    case RAD_INPUT_KEY_LEFT:
        send_terminal_bytes(pty, slave, "\x1b[D", 3);
        break;
    case RAD_INPUT_KEY_HOME:
        send_terminal_bytes(pty, slave, "\x1b[H", 3);
        break;
    case RAD_INPUT_KEY_END:
        send_terminal_bytes(pty, slave, "\x1b[F", 3);
        break;
    case RAD_INPUT_KEY_INSERT:
        send_terminal_bytes(pty, slave, "\x1b[2~", 4);
        break;
    case RAD_INPUT_KEY_DELETE:
        send_terminal_bytes(pty, slave, "\x1b[3~", 4);
        break;
    case RAD_INPUT_KEY_PAGE_UP:
        send_terminal_bytes(pty, slave, "\x1b[5~", 4);
        break;
    case RAD_INPUT_KEY_PAGE_DOWN:
        send_terminal_bytes(pty, slave, "\x1b[6~", 4);
        break;
    default:
        break;
    }
}

void pump_input_to_pty(rad_device_t input, rad_pty_t pty, rad_tty_t slave) {
    rad_input_event_t event{};
    while (rad_input_read_event(input, &event) == RAD_STATUS_OK) {
        input_event_to_pty(&event, pty, slave);
    }
}

void pump_pointer_input(rad_device_t input) {
    rad_input_event_t event{};
    while (rad_input_read_event(input, &event) == RAD_STATUS_OK) {
        if ((event.type == RAD_INPUT_EVENT_POINTER_MOTION || event.type == RAD_INPUT_EVENT_POINTER_BUTTON)
            && !g_pointer_input_seen) {
            g_pointer_input_seen = 1;
            x86_serial_write("pointer input event\n");
            rad_debug_marker("RADIX_INPUT_POINTER_EVENT_OK");
            append_text("RADIX_INPUT_POINTER_EVENT_OK\n");
        }
    }
}
}

extern "C" void kernel_main64(uint32_t magic, uint32_t info_addr) {
    x86_serial_init();
    x86_serial_write("RAD x86_64 GRUB Slint handoff\n");
    x86_serial_write("magic=");
    serial_hex64(magic);
    x86_serial_write(" info=");
    serial_hex64(info_addr);
    x86_serial_write("\n");

    if (magic != Multiboot2BootloaderMagic) {
        x86_serial_write("invalid multiboot2 magic\n");
        rad_cpu_halt_forever();
    }

    const CpuTopologyProbe topology = detect_cpu_topology(info_addr);
    x86_cpu_set_topology(topology.detected_cores, topology.present_mask, topology.lapic_address);
    if (topology.detected_cores > 1) {
        x86_serial_write("RADIX_SMP_TOPOLOGY_OK\n");
    } else {
        x86_serial_write("RADIX_SMP_TOPOLOGY_SINGLE\n");
    }

    x86_cpu_init(reinterpret_cast<uint64_t>(x86_boot_stack_top));

    const Mb2FramebufferTag *fb = find_framebuffer(info_addr);
    rad_boot_info_t boot{};
    radboot_prepare_default(&boot, "x86_64_grub", "qemu-pc", "/dev/console", "none");
    add_memory_regions(&boot, info_addr);
    radboot_add_arg(&boot, "bootloader", "grub-multiboot2");
    radboot_add_arg(&boot, "ui", "slint-freestanding");

    x86_vm_summary_t vm_summary{};
    x86_vm_init(&boot, &vm_summary);

    rad_kernel_config_t kernel_config{};
    kernel_config.backend_name = "x86_64_grub";
    kernel_config.boot_info = &boot;
    x86_serial_write("init kernel\n");
    rad_kernel_init(&kernel_config);
    x86_serial_write("kernel initialized\n");
    rad_cpu_interrupts_enable();
    if (x86_cpp_runtime_self_test()) {
        append_text("RADIX_CPP_RUNTIME_OK\n");
        rad_debug_marker("RADIX_CPP_RUNTIME_OK");
    } else {
        append_text("RADIX_CPP_RUNTIME_FAIL\n");
        rad_debug_marker("RADIX_CPP_RUNTIME_FAIL");
    }
    module_lifecycle_self_test();
    runtime_self_test();
    x86_storage_summary_t storage_summary{};
    x86_storage_probe(&storage_summary);
    x86_serial_write("register framebuffer\n");
    const rad_status_t framebuffer_register_status = register_framebuffer(fb);
    char status_line[96];
    snprintf(status_line, sizeof(status_line), "framebuffer register status=%d\n", static_cast<int>(framebuffer_register_status));
    x86_serial_write(status_line);

    x86_serial_write("attach serial\n");
    rad_terminal_attach_device("/dev/ttyS0");

    rad_framebuffer_t framebuffer = nullptr;
    const rad_status_t framebuffer_open_status = rad_framebuffer_open_primary(&framebuffer);
    snprintf(status_line, sizeof(status_line), "framebuffer open status=%d handle=0x%llx\n",
        static_cast<int>(framebuffer_open_status),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(framebuffer)));
    x86_serial_write(status_line);
    rad_framebuffer_info_t framebuffer_probe_info{};
    rad_status_t framebuffer_probe_status = rad_framebuffer_get_info(framebuffer, &framebuffer_probe_info);
    snprintf(status_line, sizeof(status_line), "framebuffer probe status=%d pixels=0x%llx\n",
        static_cast<int>(framebuffer_probe_status),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(framebuffer_probe_info.pixels)));
    x86_serial_write(status_line);

    x86_serial_write("open pty\n");
    rad_pty_t pty = nullptr;
    rad_pty_open_pair("boot-terminal", &pty);
    char slave_name[64]{};
    rad_pty_slave_name(pty, slave_name, sizeof(slave_name));
    rad_tty_t slave = nullptr;
    rad_tty_open(slave_name, &slave);
    rad_tty_window_size_t tty_size{32, 100};
    rad_pty_resize(pty, &tty_size);
    x86_serial_write("pty ready\n");

    append_text("RADKernel x86_64 boot terminal\n");
    append_text("PTY shell ready. Type commands on the VM keyboard or serial stdio.\n\n");
    x86_serial_write("cmd bootinfo\n");
    pty_command(pty, slave, "bootinfo");
    x86_serial_write("cmd devices\n");
    pty_command(pty, slave, "devices");
    x86_serial_write("cmd fb\n");
    pty_command(pty, slave, "fb");
    x86_serial_write("cmd drivers\n");
    pty_command(pty, slave, "drivers");
    x86_serial_write("cmd irq\n");
    pty_command(pty, slave, "irq");
    x86_serial_write("cmd irq-tree\n");
    pty_command(pty, slave, "irq-tree");
    x86_serial_write("cmd sched\n");
    pty_command(pty, slave, "sched");
    x86_serial_write("cmd perf\n");
    pty_command(pty, slave, "perf");
    x86_serial_write("cmd latency\n");
    pty_command(pty, slave, "latency");
    posix_abi_self_test();
    if (x86_cpu_self_test()) append_text("RADIX_INT80_OK\n");
    if (x86_context_self_test()) {
        append_text("RADIX_CONTEXT_SWITCH_OK\n");
        rad_printk("RADIX_CONTEXT_SWITCH_OK\n");
    } else {
        append_text("RADIX_CONTEXT_SWITCH_FAIL\n");
        rad_printk("RADIX_CONTEXT_SWITCH_FAIL\n");
    }
    if (x86_vm_self_test()) append_text("RADIX_VM_PAGE_ALLOC_OK\n");
    if (x86_storage_self_test()) {
        append_text("RADIX_VIRTIO_BLK_READ_OK\n");
        if (x86_ext4_mount_root("/dev/vda") == RAD_STATUS_OK) {
            append_text("RADIX_EXT4_MOUNT_OK\n");
            if (x86_ext4_self_test()) {
                append_text("RADIX_EXT4_ROOTFS_OK\n");
                append_text("RADIX_EXT4_RW_OK\n");
                append_text("RADIX_ROOTFS_INIT_FOUND\n");
                if (x86_user_run_init("/bin/init")) append_text("RADIX_USER_INIT_OK\n");
            }
        }
    }
    if (x86_fat_mount("/dev/vdb", "/mnt/fat") == RAD_STATUS_OK) {
        append_text("RADIX_FAT_MOUNT_OK\n");
        if (x86_fat_self_test()) append_text("RADIX_FAT_RW_OK\n");
    } else {
        rad_debug_marker("RADIX_FAT_MOUNT_FAIL");
    }
    if (x86_network_self_test()) {
        append_text("RADIX_ETH_TX_OK\n");
        append_text("RADIX_ARP_OK\n");
        append_text("RADIX_IPV4_OK\n");
        append_text("RADIX_UDP_OK\n");
    }
    append_text(vm_summary.user_address_space_ready ? "RADIX_USER_VM_SCAFFOLD_OK\n" : "RADIX_USER_VM_SCAFFOLD_FAIL\n");
    append_text(storage_summary.virtio_block_devices ? "RADIX_VIRTIO_BLK_FOUND\n" : "RADIX_VIRTIO_BLK_ABSENT\n");
    x86_serial_write("start slint shell\n");
    framebuffer_probe_info = {};
    framebuffer_probe_status = rad_framebuffer_get_info(framebuffer, &framebuffer_probe_info);
    snprintf(status_line, sizeof(status_line), "framebuffer pre-slint status=%d pixels=0x%llx\n",
        static_cast<int>(framebuffer_probe_status),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(framebuffer_probe_info.pixels)));
    x86_serial_write(status_line);
    rad_status_t slint_status = framebuffer
        ? radix_slint_shell_start(framebuffer, g_transcript)
        : RAD_STATUS_NOT_FOUND;
    if (slint_status == RAD_STATUS_OK) {
        x86_serial_write("RAD x86_64 Slint terminal ready\n");
    } else {
        append_text("RADIX_SLINT_BOOT_SHELL_FAIL\n");
        rad_debug_marker("RADIX_SLINT_BOOT_SHELL_FAIL");
        char slint_status_line[80];
        snprintf(slint_status_line, sizeof(slint_status_line), "RADIX_SLINT_STATUS %d\n", static_cast<int>(slint_status));
        x86_serial_write(slint_status_line);
        if (framebuffer) render_terminal(framebuffer);
        x86_serial_write("RAD x86_64 Slint terminal failed\n");
    }

    rad_device_t serial = nullptr;
    rad_device_open("/dev/ttyS0", &serial);
    rad_device_t input = nullptr;
    if (rad_input_open("/dev/input/event0", &input) == RAD_STATUS_OK) {
        append_text("\nkeyboard: /dev/input/event0 ready\n");
        if (radix_slint_shell_ready()) radix_slint_shell_set_terminal_text(g_transcript);
        else if (framebuffer) render_terminal(framebuffer);
    } else {
        append_text("\nkeyboard: /dev/input/event0 unavailable\n");
        if (radix_slint_shell_ready()) radix_slint_shell_set_terminal_text(g_transcript);
        else if (framebuffer) render_terminal(framebuffer);
    }
    rad_device_t pointer = nullptr;
    if (rad_input_open("/dev/input/event1", &pointer) == RAD_STATUS_OK) {
        append_text("pointer: /dev/input/event1 ready\n");
        if (radix_slint_shell_ready()) radix_slint_shell_set_terminal_text(g_transcript);
        else if (framebuffer) render_terminal(framebuffer);
    } else {
        append_text("pointer: /dev/input/event1 unavailable\n");
        if (radix_slint_shell_ready()) radix_slint_shell_set_terminal_text(g_transcript);
        else if (framebuffer) render_terminal(framebuffer);
    }
    for (;;) {
        if (serial) pump_serial_to_pty(serial, pty, slave);
        if (input) pump_input_to_pty(input, pty, slave);
        if (pointer) pump_pointer_input(pointer);
        if (radix_slint_shell_ready()) {
            radix_slint_shell_set_terminal_text(g_transcript);
            radix_slint_shell_poll();
        } else if (framebuffer) {
            render_terminal(framebuffer);
        }
        rad_kernel_poll();
        rad_sleep_ms(16);
    }
}
