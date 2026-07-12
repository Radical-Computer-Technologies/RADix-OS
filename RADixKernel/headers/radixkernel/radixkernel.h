#ifndef RADEMBEDDEDKERNEL_H
#define RADEMBEDDEDKERNEL_H

/**
 * @file radixkernel.h
 * @brief Portable embedded kernel service ABI for RADLib.
 *
 * RADixKernel exposes a small POSIX-like service contract for bare-metal
 * and simulator backends. The C ABI is the stable boundary; the C++ wrappers are
 * convenience helpers for RADLib applications and examples.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#if defined(__cplusplus) && !defined(RADLIB_EMBEDDED_NO_CPP_WRAPPERS)
#include <string>
#include <vector>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RADEK_VERSION_MAJOR 0
#define RADEK_VERSION_MINOR 1
#define RADEK_VERSION_PATCH 2

#define RAD_IOCTL_NONE 0u
#define RAD_IOCTL_WRITE 1u
#define RAD_IOCTL_READ 2u
#define RAD_IOCTL_READWRITE 3u
#define RAD_IOCTL(dir, type, nr, size) ((((uint32_t)(dir) & 3u) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | (((uint32_t)(type) & 0xffu) << 8) | ((uint32_t)(nr) & 0xffu))
#define RAD_IO(type, nr) RAD_IOCTL(RAD_IOCTL_NONE, type, nr, 0u)
#define RAD_IOR(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_READ, type, nr, sizeof(type_name))
#define RAD_IOW(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_WRITE, type, nr, sizeof(type_name))
#define RAD_IOWR(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_READWRITE, type, nr, sizeof(type_name))
#define RAD_IOCTL_DIR(request) (((uint32_t)(request) >> 30) & 3u)
#define RAD_IOCTL_SIZE(request) (((uint32_t)(request) >> 16) & 0x3fffu)
#define RAD_IOCTL_TYPE(request) (((uint32_t)(request) >> 8) & 0xffu)
#define RAD_IOCTL_NR(request) ((uint32_t)(request) & 0xffu)

#define RAD_IOCTL_TYPE_I2C 'I'
#define RAD_IOCTL_TYPE_SPI 'S'
#define RAD_IOCTL_TYPE_AUDIO 'A'
#define RAD_IOCTL_TYPE_SERIAL 'T'
#define RAD_IOCTL_TYPE_FRAMEBUFFER 'F'
#define RAD_IOCTL_TYPE_INPUT 'K'
#define RAD_IOCTL_TYPE_BLOCK 'B'
#define RAD_IOCTL_TYPE_NET 'N'

#define RAD_DEVICE_IOCTL_I2C_TRANSFER RAD_IOWR(RAD_IOCTL_TYPE_I2C, 1u, struct rad_i2c_transfer)
#define RAD_DEVICE_IOCTL_SPI_TRANSFER RAD_IOWR(RAD_IOCTL_TYPE_SPI, 1u, struct rad_spi_transfer)
#define RAD_DEVICE_IOCTL_AUDIO_CONFIGURE RAD_IOW(RAD_IOCTL_TYPE_AUDIO, 1u, struct rad_audio_format)
#define RAD_DEVICE_IOCTL_SERIAL_CONFIGURE RAD_IOW(RAD_IOCTL_TYPE_SERIAL, 1u, struct rad_serial_config)
#define RAD_DEVICE_IOCTL_FRAMEBUFFER_INFO RAD_IOR(RAD_IOCTL_TYPE_FRAMEBUFFER, 1u, struct rad_framebuffer_info)
#define RAD_DEVICE_IOCTL_FRAMEBUFFER_FLUSH RAD_IOW(RAD_IOCTL_TYPE_FRAMEBUFFER, 2u, struct rad_framebuffer_rect)
#define RAD_DEVICE_IOCTL_BLOCK_INFO RAD_IOR(RAD_IOCTL_TYPE_BLOCK, 1u, struct rad_block_info)
#define RAD_DEVICE_IOCTL_BLOCK_READ RAD_IOWR(RAD_IOCTL_TYPE_BLOCK, 2u, struct rad_block_request)
#define RAD_DEVICE_IOCTL_BLOCK_WRITE RAD_IOWR(RAD_IOCTL_TYPE_BLOCK, 3u, struct rad_block_request)
#define RAD_DEVICE_IOCTL_BLOCK_FLUSH RAD_IO(RAD_IOCTL_TYPE_BLOCK, 4u)
#define RAD_DEVICE_IOCTL_NET_LINK_INFO RAD_IOR(RAD_IOCTL_TYPE_NET, 1u, struct rad_net_link_info)
#define RAD_DEVICE_IOCTL_NET_SEND RAD_IOW(RAD_IOCTL_TYPE_NET, 2u, struct rad_net_packet)
#define RAD_DEVICE_IOCTL_NET_POLL RAD_IO(RAD_IOCTL_TYPE_NET, 3u)

#define RAD_BOOT_MAX_ARGS 16u
#define RAD_BOOT_MAX_MEMORY_REGIONS 8u
#define RAD_BOOT_MAX_STRING 64u

#define RAD_OVERLAY_MAGIC 0x4f444152u
#define RAD_OVERLAY_VERSION 1u
#define RAD_TREE_MAX_PROPERTY_NAME 64u
#define RAD_TREE_MAX_PATH 96u
#define RAD_TREE_MAX_VALUE 96u
#define RAD_I2C_BUS_NAME_MAX 32u
#define RAD_SPI_BUS_NAME_MAX 32u
#define RAD_DRIVER_NAME_MAX 32u
#define RAD_COMPATIBLE_MAX 64u
#define RAD_MODULE_NAME_MAX 32u
#define RAD_MODULE_MAX_INFO 32u
#define RAD_IRQ_NAME_MAX 32u
#define RAD_IRQ_DOMAIN_NAME_MAX 32u
#define RAD_IRQ_MAX_RESOURCES 4u
#define RAD_FRAMEBUFFER_NAME_MAX 64u
#define RAD_DISPLAY_CONNECTOR_MAX 32u
#define RAD_FRAMEBUFFER_MAX_MODES 8u
#define RAD_PERF_NAME_MAX 32u
#define RAD_TIMER_NAME_MAX 32u
#define RAD_INPUT_QUEUE_NAME_MAX 64u
#define RAD_WAIT_FOREVER 0xffffffffu

#define RAD_BOOT_MEMORY_USABLE 1u
#define RAD_BOOT_MEMORY_RESERVED 2u
#define RAD_BOOT_MEMORY_PSRAM 3u
#define RAD_BOOT_MEMORY_MMIO 4u

typedef enum rad_status {
    RAD_STATUS_OK = 0,
    RAD_STATUS_ERROR = -1,
    RAD_STATUS_INVALID_ARGUMENT = -2,
    RAD_STATUS_NOT_FOUND = -3,
    RAD_STATUS_NO_MEMORY = -4,
    RAD_STATUS_TIMEOUT = -5,
    RAD_STATUS_NOT_SUPPORTED = -6,
    RAD_STATUS_ALREADY_EXISTS = -7,
    RAD_STATUS_NOT_INITIALIZED = -8
} rad_status_t;

typedef enum rad_posix_syscall {
    RAD_SYSCALL_READ = 0,
    RAD_SYSCALL_WRITE = 1,
    RAD_SYSCALL_OPEN = 2,
    RAD_SYSCALL_CLOSE = 3,
    RAD_SYSCALL_IOCTL = 4,
    RAD_SYSCALL_LSEEK = 5,
    RAD_SYSCALL_STAT = 6,
    RAD_SYSCALL_FSTAT = 7,
    RAD_SYSCALL_GETTIMEOFDAY = 8,
    RAD_SYSCALL_NANOSLEEP = 9,
    RAD_SYSCALL_EXIT = 10,
    RAD_SYSCALL_FORK = 11,
    RAD_SYSCALL_EXECVE = 12,
    RAD_SYSCALL_WAITPID = 13,
    RAD_SYSCALL_GETPID = 14,
    RAD_SYSCALL_GETPPID = 15,
    RAD_SYSCALL_DUP = 16,
    RAD_SYSCALL_DUP2 = 17,
    RAD_SYSCALL_CHDIR = 18,
    RAD_SYSCALL_GETCWD = 19,
    RAD_SYSCALL_BRK = 20,
    RAD_SYSCALL_PIPE = 21,
    RAD_SYSCALL_FCNTL = 22,
    RAD_SYSCALL_ACCESS = 23,
    RAD_SYSCALL_ISATTY = 24,
    RAD_SYSCALL_SOCKET = 25,
    RAD_SYSCALL_BIND = 26,
    RAD_SYSCALL_CONNECT = 27,
    RAD_SYSCALL_LISTEN = 28,
    RAD_SYSCALL_ACCEPT = 29,
    RAD_SYSCALL_SENDTO = 30,
    RAD_SYSCALL_RECVFROM = 31,
    RAD_SYSCALL_SHUTDOWN = 32,
    RAD_SYSCALL_SETSOCKOPT = 33,
    RAD_SYSCALL_GETSOCKOPT = 34
} rad_posix_syscall_t;

typedef enum rad_process_state {
    RAD_PROCESS_RUNNING = 1,
    RAD_PROCESS_WAITING = 2,
    RAD_PROCESS_ZOMBIE = 3
} rad_process_state_t;

#define RAD_WAIT_NOHANG 1u
#define RAD_FD_CLOEXEC 1u
#define RAD_FCNTL_GETFD 1u
#define RAD_FCNTL_SETFD 2u
#define RAD_FCNTL_GETFL 3u

typedef enum rad_vfs_open_flags {
    RAD_VFS_READ = 1u << 0,
    RAD_VFS_WRITE = 1u << 1,
    RAD_VFS_CREATE = 1u << 2,
    RAD_VFS_TRUNCATE = 1u << 3,
    RAD_VFS_APPEND = 1u << 4
} rad_vfs_open_flags_t;

typedef enum rad_seek_origin {
    RAD_SEEK_SET = 0,
    RAD_SEEK_CUR = 1,
    RAD_SEEK_END = 2
} rad_seek_origin_t;

typedef enum rad_device_type {
    RAD_DEVICE_GENERIC = 0,
    RAD_DEVICE_I2C = 1,
    RAD_DEVICE_SPI = 2,
    RAD_DEVICE_AUDIO = 3,
    RAD_DEVICE_SERIAL = 4,
    RAD_DEVICE_FRAMEBUFFER = 5,
    RAD_DEVICE_TTY = 6,
    RAD_DEVICE_PTY = 7,
    RAD_DEVICE_INPUT = 8,
    RAD_DEVICE_BLOCK = 9,
    RAD_DEVICE_NETWORK = 10
} rad_device_type_t;

typedef enum rad_input_event_type {
    RAD_INPUT_EVENT_NONE = 0,
    RAD_INPUT_EVENT_KEY = 1,
    RAD_INPUT_EVENT_POINTER_MOTION = 2,
    RAD_INPUT_EVENT_POINTER_BUTTON = 3,
    RAD_INPUT_EVENT_POINTER_SCROLL = 4
} rad_input_event_type_t;

typedef enum rad_input_key {
    RAD_INPUT_KEY_UNKNOWN = 0,
    RAD_INPUT_KEY_ESCAPE = 1,
    RAD_INPUT_KEY_ENTER = 2,
    RAD_INPUT_KEY_BACKSPACE = 3,
    RAD_INPUT_KEY_TAB = 4,
    RAD_INPUT_KEY_UP = 5,
    RAD_INPUT_KEY_DOWN = 6,
    RAD_INPUT_KEY_LEFT = 7,
    RAD_INPUT_KEY_RIGHT = 8,
    RAD_INPUT_KEY_HOME = 9,
    RAD_INPUT_KEY_END = 10,
    RAD_INPUT_KEY_PAGE_UP = 11,
    RAD_INPUT_KEY_PAGE_DOWN = 12,
    RAD_INPUT_KEY_INSERT = 13,
    RAD_INPUT_KEY_DELETE = 14,
    RAD_INPUT_KEY_LEFT_SHIFT = 15,
    RAD_INPUT_KEY_RIGHT_SHIFT = 16,
    RAD_INPUT_KEY_LEFT_CTRL = 17,
    RAD_INPUT_KEY_RIGHT_CTRL = 18,
    RAD_INPUT_KEY_LEFT_ALT = 19,
    RAD_INPUT_KEY_RIGHT_ALT = 20,
    RAD_INPUT_KEY_CAPS_LOCK = 21
} rad_input_key_t;

typedef enum rad_input_modifier_flags {
    RAD_INPUT_MOD_SHIFT = 1u << 0,
    RAD_INPUT_MOD_CTRL = 1u << 1,
    RAD_INPUT_MOD_ALT = 1u << 2,
    RAD_INPUT_MOD_META = 1u << 3,
    RAD_INPUT_MOD_CAPS_LOCK = 1u << 4
} rad_input_modifier_flags_t;

typedef enum rad_input_pointer_button_flags {
    RAD_INPUT_POINTER_BUTTON_LEFT = 1u << 0,
    RAD_INPUT_POINTER_BUTTON_RIGHT = 1u << 1,
    RAD_INPUT_POINTER_BUTTON_MIDDLE = 1u << 2
} rad_input_pointer_button_flags_t;

typedef enum rad_pixel_format {
    RAD_PIXEL_FORMAT_RGB565 = 1,
    RAD_PIXEL_FORMAT_XRGB8888 = 2
} rad_pixel_format_t;

typedef enum rad_sd_mode {
    RAD_SD_MODE_AUTO = 0,
    RAD_SD_MODE_SPI = 1,
    RAD_SD_MODE_PIO_SDIO = 2
} rad_sd_mode_t;

typedef enum rad_spi_transfer_mode {
    RAD_SPI_TRANSFER_MODE_AUTO = 0,
    RAD_SPI_TRANSFER_MODE_PIO = 1,
    RAD_SPI_TRANSFER_MODE_DMA = 2
} rad_spi_transfer_mode_t;

typedef enum rad_dma_transfer_type {
    RAD_DMA_MEMORY_TO_MEMORY = 0,
    RAD_DMA_MEMORY_TO_DEVICE = 1,
    RAD_DMA_DEVICE_TO_MEMORY = 2
} rad_dma_transfer_type_t;

typedef enum rad_dma_request_id {
    RAD_DMA_DREQ_NONE = 0,
    RAD_DMA_DREQ_SPI0_TX = 1,
    RAD_DMA_DREQ_SPI0_RX = 2,
    RAD_DMA_DREQ_SPI1_TX = 3,
    RAD_DMA_DREQ_SPI1_RX = 4,
    RAD_DMA_DREQ_RP2350_HSTX_TX = 5
} rad_dma_request_id_t;

typedef enum rad_display_output_type {
    RAD_DISPLAY_OUTPUT_MEMORY = 0,
    RAD_DISPLAY_OUTPUT_CIRCLE = 1,
    RAD_DISPLAY_OUTPUT_GRUB = 2,
    RAD_DISPLAY_OUTPUT_RP2350_HSTX = 3,
    RAD_DISPLAY_OUTPUT_SPI_PANEL = 4
} rad_display_output_type_t;

typedef enum rad_tty_mode_flags {
    RAD_TTY_MODE_CANONICAL = 1u << 0,
    RAD_TTY_MODE_ECHO = 1u << 1,
    RAD_TTY_MODE_CRLF = 1u << 2
} rad_tty_mode_flags_t;

typedef enum rad_irq_trigger {
    RAD_IRQ_TRIGGER_NONE = 0,
    RAD_IRQ_TRIGGER_EDGE_RISING = 1u << 0,
    RAD_IRQ_TRIGGER_EDGE_FALLING = 1u << 1,
    RAD_IRQ_TRIGGER_LEVEL_HIGH = 1u << 2,
    RAD_IRQ_TRIGGER_LEVEL_LOW = 1u << 3
} rad_irq_trigger_t;

#define RAD_TASK_CORE_ANY (-1)
#define RAD_TASK_CORE_SERVICE 0

typedef enum rad_task_state {
    RAD_TASK_NEW = 0,
    RAD_TASK_READY = 1,
    RAD_TASK_RUNNING = 2,
    RAD_TASK_SLEEPING = 3,
    RAD_TASK_BLOCKED = 4,
    RAD_TASK_FINISHED = 5,
    RAD_TASK_DETACHED = 6
} rad_task_state_t;

typedef struct rad_kernel_config {
    const char *backend_name;
    const void *boot_info;
} rad_kernel_config_t;

typedef struct rad_boot_arg {
    char key[RAD_BOOT_MAX_STRING];
    char value[RAD_BOOT_MAX_STRING];
} rad_boot_arg_t;

typedef struct rad_boot_memory_region {
    uint64_t base;
    uint64_t size;
    uint32_t type;
    uint32_t flags;
    char name[RAD_BOOT_MAX_STRING];
} rad_boot_memory_region_t;

typedef struct rad_boot_info {
    uint32_t size;
    uint32_t version;
    char backend[RAD_BOOT_MAX_STRING];
    char board[RAD_BOOT_MAX_STRING];
    char console_device[RAD_BOOT_MAX_STRING];
    char sd_mode[RAD_BOOT_MAX_STRING];
    uintptr_t fdt_pointer;
    uintptr_t initrd_pointer;
    uint32_t boot_flags;
    uint32_t memory_region_count;
    rad_boot_memory_region_t memory_regions[RAD_BOOT_MAX_MEMORY_REGIONS];
    uint32_t arg_count;
    rad_boot_arg_t args[RAD_BOOT_MAX_ARGS];
} rad_boot_info_t;

typedef struct rad_memory_stats {
    uint64_t allocations;
    uint64_t frees;
    uint64_t bytes_allocated;
    uint64_t bytes_freed;
    uint64_t bytes_live;
    uint64_t peak_bytes_live;
} rad_memory_stats_t;

typedef struct rad_perf_counter_info {
    char name[RAD_PERF_NAME_MAX];
    uint64_t value;
} rad_perf_counter_info_t;

typedef struct rad_timer_source_config {
    uint32_t size;
    const char *name;
    uint32_t frequency_hz;
    int supports_oneshot;
} rad_timer_source_config_t;

typedef struct rad_timer_source_ops {
    void *context;
    rad_status_t (*start_periodic)(void *context, uint32_t frequency_hz);
    rad_status_t (*schedule_oneshot)(void *context, uint64_t delay_micros);
    rad_status_t (*cancel_oneshot)(void *context);
} rad_timer_source_ops_t;

typedef struct rad_timer_source_info {
    char name[RAD_TIMER_NAME_MAX];
    uint32_t frequency_hz;
    int supports_oneshot;
    int active;
    uint64_t ticks;
} rad_timer_source_info_t;

typedef struct rad_task_info {
    uint64_t id;
    char name[64];
    int running;
    int finished;
    rad_task_state_t state;
    int target_core;
    int current_core;
    int priority;
    size_t stack_size;
    int detached;
} rad_task_info_t;

typedef struct rad_task_config {
    uint32_t size;
    const char *name;
    size_t stack_size;
    int target_core;
    int priority;
    void *user_context;
} rad_task_config_t;

typedef struct rad_core_info {
    uint32_t detected_cores;
    uint32_t worker_cores;
    uint32_t service_core;
    uint32_t worker_running_mask;
} rad_core_info_t;

typedef enum rad_scheduler_mode {
    RAD_SCHEDULER_COOPERATIVE = 0,
    RAD_SCHEDULER_PREEMPTIVE = 1
} rad_scheduler_mode_t;

typedef struct rad_scheduler_info {
    uint32_t detected_cores;
    uint32_t worker_running_mask;
    uint32_t running_threads;
    uint32_t ready_threads;
    uint32_t blocked_threads;
    uint32_t sleeping_threads;
    uint32_t exited_threads;
    uint32_t process_count;
    int preemption_supported;
    rad_scheduler_mode_t mode;
    char arch[32];
    uint32_t online_core_mask;
    uint32_t current_core;
    int preemption_enabled;
    uint64_t context_switches;
    uint64_t scheduler_ticks;
} rad_scheduler_info_t;

typedef enum rad_tree_property_type {
    RAD_TREE_PROP_BOOL = 1,
    RAD_TREE_PROP_U32 = 2,
    RAD_TREE_PROP_STRING = 3,
    RAD_TREE_PROP_U32_ARRAY = 4,
    RAD_TREE_PROP_STRING_ARRAY = 5
} rad_tree_property_type_t;

typedef struct rad_tree_node_handle *rad_tree_node_t;

typedef struct rad_tree_node_info {
    char path[RAD_TREE_MAX_PATH];
    char name[RAD_TREE_MAX_PROPERTY_NAME];
    char parent[RAD_TREE_MAX_PATH];
    char module[RAD_TREE_MAX_PROPERTY_NAME];
    int bound;
} rad_tree_node_info_t;

typedef struct rad_overlay_info {
    char name[RAD_TREE_MAX_PROPERTY_NAME];
    char source[RAD_TREE_MAX_PATH];
    uint32_t node_count;
    uint32_t property_count;
} rad_overlay_info_t;

typedef struct rad_vfs_stat {
    uint64_t size;
    int is_directory;
    uint32_t mode;
    uint64_t mtime_millis;
} rad_vfs_stat_t;

typedef struct rad_block_info {
    uint32_t size;
    uint32_t sector_size;
    uint64_t sector_count;
    uint32_t flags;
    uint32_t reserved;
} rad_block_info_t;

typedef struct rad_block_request {
    uint32_t size;
    uint64_t sector;
    uint32_t sector_count;
    void *buffer;
} rad_block_request_t;

typedef struct rad_mac_address {
    uint8_t bytes[6];
} rad_mac_address_t;

typedef struct rad_ipv4_address {
    uint8_t bytes[4];
} rad_ipv4_address_t;

typedef struct rad_net_packet {
    uint32_t size;
    const void *data;
    size_t length;
} rad_net_packet_t;

typedef struct rad_net_link_info {
    uint32_t size;
    rad_mac_address_t mac;
    uint32_t mtu;
    int link_up;
    uint64_t tx_packets;
    uint64_t rx_packets;
} rad_net_link_info_t;

typedef struct rad_posix_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
} rad_posix_timeval_t;

typedef struct rad_process_info {
    int32_t pid;
    int32_t parent_pid;
    int32_t exited;
    int32_t exit_code;
    rad_process_state_t state;
    char path[128];
} rad_process_info_t;

typedef struct rad_process_arch_ops {
    uint32_t size;
    void *context;
    int32_t (*fork_from_frame)(void *context, void *trap_frame);
    void (*process_reaped)(void *context, int32_t pid, int32_t status);
} rad_process_arch_ops_t;

typedef struct rad_vfs_dirent {
    char name[96];
    rad_vfs_stat_t stat;
} rad_vfs_dirent_t;

typedef int (*rad_vfs_list_callback_t)(const char *name, const rad_vfs_stat_t *stat, void *context);

typedef struct rad_vfs_backend_ops {
    void *context;
    rad_status_t (*open)(void *context, const char *path, uint32_t flags, void **file);
    rad_status_t (*read)(void *file, void *buffer, size_t size, size_t *bytes_read);
    rad_status_t (*write)(void *file, const void *buffer, size_t size, size_t *bytes_written);
    rad_status_t (*seek)(void *file, int64_t offset, rad_seek_origin_t origin);
    uint64_t (*tell)(void *file);
    void (*close)(void *file);
    rad_status_t (*stat)(void *context, const char *path, rad_vfs_stat_t *stat);
    rad_status_t (*list)(void *context, const char *path, rad_vfs_list_callback_t callback, void *callback_context);
    rad_status_t (*mkdir)(void *context, const char *path);
    rad_status_t (*remove)(void *context, const char *path);
    rad_status_t (*rename)(void *context, const char *old_path, const char *new_path);
    rad_status_t (*rmdir)(void *context, const char *path);
    rad_status_t (*fsync)(void *file);
    rad_status_t (*truncate)(void *context, const char *path, uint64_t size);
    rad_status_t (*readlink)(void *context, const char *path, char *buffer, size_t size);
    rad_status_t (*symlink)(void *context, const char *target, const char *link_path);
    rad_status_t (*link)(void *context, const char *old_path, const char *new_path);
    rad_status_t (*chmod)(void *context, const char *path, uint32_t mode);
} rad_vfs_backend_ops_t;

typedef enum rad_program_state {
    RAD_PROGRAM_LOADED = 0,
    RAD_PROGRAM_RUNNING = 1,
    RAD_PROGRAM_FINISHED = 2,
    RAD_PROGRAM_FAILED = 3
} rad_program_state_t;

typedef struct rad_program_info {
    uint64_t id;
    char path[128];
    char name[64];
    rad_program_state_t state;
    uint64_t task_id;
    int exit_code;
} rad_program_info_t;

typedef struct rad_device_ops {
    void *context;
    rad_status_t (*read)(void *context, void *buffer, size_t size, size_t *bytes_read);
    rad_status_t (*write)(void *context, const void *buffer, size_t size, size_t *bytes_written);
    rad_status_t (*ioctl)(void *context, uint32_t request, void *argument);
} rad_device_ops_t;

typedef struct rad_input_event {
    uint32_t size;
    rad_input_event_type_t type;
    uint32_t code;
    uint32_t codepoint;
    uint32_t modifiers;
    uint8_t pressed;
    uint8_t repeat;
    uint16_t reserved;
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    int32_t scroll_x;
    int32_t scroll_y;
    uint32_t buttons;
} rad_input_event_t;

typedef struct rad_framebuffer_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} rad_framebuffer_rect_t;

typedef struct rad_framebuffer_info {
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride_bytes;
    rad_pixel_format_t pixel_format;
    void *pixels;
} rad_framebuffer_info_t;

typedef struct rad_display_mode {
    uint32_t width;
    uint32_t height;
    uint32_t refresh_hz;
    uint32_t stride_bytes;
    rad_pixel_format_t pixel_format;
} rad_display_mode_t;

typedef struct rad_framebuffer_ops {
    void *context;
    rad_status_t (*flush)(void *context, const rad_framebuffer_rect_t *rect);
    rad_status_t (*set_mode)(void *context, const rad_display_mode_t *mode);
    rad_status_t (*blank)(void *context, int blanked);
    uint64_t (*get_vsync_counter)(void *context);
} rad_framebuffer_ops_t;

typedef struct rad_framebuffer_handle *rad_framebuffer_t;

typedef struct rad_framebuffer_config {
    uint32_t size;
    const char *name;
    rad_framebuffer_info_t info;
    rad_display_output_type_t output_type;
    const char *connector;
    uint32_t mode_count;
    uint32_t preferred_mode;
    rad_display_mode_t modes[RAD_FRAMEBUFFER_MAX_MODES];
    int primary;
} rad_framebuffer_config_t;

typedef struct rad_framebuffer_display_info {
    uint32_t size;
    char name[RAD_FRAMEBUFFER_NAME_MAX];
    rad_framebuffer_info_t framebuffer;
    rad_display_output_type_t output_type;
    char connector[RAD_DISPLAY_CONNECTOR_MAX];
    uint32_t mode_count;
    uint32_t preferred_mode;
    uint32_t current_mode;
    rad_display_mode_t modes[RAD_FRAMEBUFFER_MAX_MODES];
    int primary;
} rad_framebuffer_display_info_t;

typedef struct rad_i2c_transfer {
    uint8_t address;
    const uint8_t *write_data;
    size_t write_size;
    uint8_t *read_data;
    size_t read_size;
} rad_i2c_transfer_t;

typedef struct rad_i2c_bus_handle *rad_i2c_bus_t;
typedef struct rad_i2c_device rad_i2c_device_t;

typedef struct rad_i2c_controller_ops {
    void *context;
    rad_status_t (*transfer)(void *context, const rad_i2c_transfer_t *transfer);
} rad_i2c_controller_ops_t;

typedef struct rad_i2c_controller_config {
    uint32_t size;
    uint32_t bus_id;
    const char *name;
    const char *tree_path;
    uint32_t clock_hz;
    uint8_t sda_gpio;
    uint8_t scl_gpio;
} rad_i2c_controller_config_t;

typedef struct rad_i2c_driver {
    uint32_t size;
    const char *name;
    const char *compatible;
    void *context;
    rad_status_t (*probe)(void *context, rad_i2c_device_t *device);
    void (*remove)(void *context, rad_i2c_device_t *device);
} rad_i2c_driver_t;

typedef struct rad_spi_transfer {
    const uint8_t *tx_data;
    uint8_t *rx_data;
    size_t size;
    uint32_t speed_hz;
    uint8_t mode;
    uint8_t bits_per_word;
    uint8_t cs;
    uint8_t transfer_mode;
} rad_spi_transfer_t;

typedef struct rad_spi_bus_handle *rad_spi_bus_t;
typedef struct rad_spi_device rad_spi_device_t;

typedef struct rad_dma_channel_handle *rad_dma_channel_t;

typedef struct rad_dma_transfer {
    uint32_t size;
    rad_dma_transfer_type_t type;
    rad_dma_request_id_t request_id;
    const void *source;
    void *destination;
    uintptr_t peripheral_address;
    uint32_t flags;
} rad_dma_transfer_t;

typedef struct rad_dma_backend_ops {
    void *context;
    rad_status_t (*request)(void *context, rad_dma_request_id_t request_id, void **backend_channel);
    void (*release)(void *context, void *backend_channel);
    rad_status_t (*submit)(void *context, void *backend_channel, const rad_dma_transfer_t *transfer);
    rad_status_t (*wait)(void *context, void *backend_channel, uint32_t timeout_ms);
    rad_status_t (*cancel)(void *context, void *backend_channel);
} rad_dma_backend_ops_t;

typedef struct rad_dma_controller_config {
    uint32_t size;
    const char *name;
    uint32_t channel_count;
} rad_dma_controller_config_t;

typedef struct rad_spi_controller_ops {
    void *context;
    rad_status_t (*transfer)(void *context, const rad_spi_device_t *device, const rad_spi_transfer_t *transfer);
    rad_status_t (*transfer_dma)(void *context, const rad_spi_device_t *device, const rad_spi_transfer_t *transfer, rad_dma_channel_t tx_channel, rad_dma_channel_t rx_channel);
} rad_spi_controller_ops_t;

typedef struct rad_spi_controller_config {
    uint32_t size;
    uint32_t bus_id;
    const char *name;
    const char *tree_path;
    uint32_t clock_hz;
    uint8_t sck_gpio;
    uint8_t tx_gpio;
    uint8_t rx_gpio;
    uint8_t cs_gpio;
} rad_spi_controller_config_t;

typedef struct rad_spi_driver {
    uint32_t size;
    const char *name;
    const char *compatible;
    void *context;
    rad_status_t (*probe)(void *context, rad_spi_device_t *device);
    void (*remove)(void *context, rad_spi_device_t *device);
} rad_spi_driver_t;

typedef struct rad_module_descriptor {
    uint32_t size;
    const char *name;
    const char *bus;
    const char *compatible;
    rad_status_t (*init)(void *context);
    void (*exit)(void *context);
    void *context;
} rad_module_descriptor_t;

typedef enum rad_module_state {
    RAD_MODULE_REGISTERED = 1,
    RAD_MODULE_INITIALIZED = 2,
    RAD_MODULE_FAILED = 3,
    RAD_MODULE_EXITED = 4
} rad_module_state_t;

typedef struct rad_module_info {
    char name[RAD_MODULE_NAME_MAX];
    char bus[RAD_DRIVER_NAME_MAX];
    char compatible[RAD_COMPATIBLE_MAX];
    rad_module_state_t state;
    int32_t last_status;
} rad_module_info_t;

typedef struct rad_driver_info {
    char name[RAD_DRIVER_NAME_MAX];
    char bus[RAD_DRIVER_NAME_MAX];
    char compatible[RAD_COMPATIBLE_MAX];
    char role[RAD_DRIVER_NAME_MAX];
} rad_driver_info_t;

typedef struct rad_irq_domain_handle *rad_irq_domain_t;

typedef struct rad_irq_domain_config {
    uint32_t size;
    const char *name;
    const char *tree_path;
    uint32_t interrupt_base;
    uint32_t line_count;
    uint32_t interrupt_cells;
} rad_irq_domain_config_t;

typedef struct rad_irq_resource {
    uint32_t irq;
    uint32_t line;
    uint32_t flags;
    rad_irq_trigger_t trigger;
    char domain[RAD_IRQ_DOMAIN_NAME_MAX];
    char controller_path[RAD_TREE_MAX_PATH];
} rad_irq_resource_t;

typedef struct rad_i2c_device_info {
    uint32_t bus_id;
    uint8_t address;
    uint32_t irq_count;
    rad_irq_resource_t irqs[RAD_IRQ_MAX_RESOURCES];
    char path[RAD_TREE_MAX_PATH];
    char compatible[RAD_COMPATIBLE_MAX];
    char driver[RAD_DRIVER_NAME_MAX];
    int bound;
} rad_i2c_device_info_t;

typedef struct rad_spi_device_info {
    uint32_t bus_id;
    uint8_t cs;
    uint32_t clock_hz;
    uint8_t mode;
    uint8_t bits_per_word;
    uint8_t transfer_mode;
    uint32_t irq_count;
    rad_irq_resource_t irqs[RAD_IRQ_MAX_RESOURCES];
    char path[RAD_TREE_MAX_PATH];
    char compatible[RAD_COMPATIBLE_MAX];
    char driver[RAD_DRIVER_NAME_MAX];
    int bound;
} rad_spi_device_info_t;

typedef struct rad_dma_channel_info {
    char controller[RAD_DRIVER_NAME_MAX];
    uint32_t index;
    uint32_t request_id;
    int allocated;
    int active;
} rad_dma_channel_info_t;

typedef struct rad_irq_info {
    uint32_t irq;
    char name[RAD_IRQ_NAME_MAX];
    int registered;
    int enabled;
    uint64_t count;
    uint64_t unhandled_count;
} rad_irq_info_t;

typedef struct rad_audio_format {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
} rad_audio_format_t;

typedef struct rad_serial_config {
    uint32_t baud_rate;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t flow_control;
} rad_serial_config_t;

typedef struct rad_sd_config {
    rad_sd_mode_t mode;
    const char *device_name;
    const char *mount_point;
    uint8_t spi_instance;
    uint8_t sck_gpio;
    uint8_t tx_gpio;
    uint8_t rx_gpio;
    uint8_t cs_gpio;
    uint8_t pio_instance;
    uint8_t sdio_clk_gpio;
    uint8_t sdio_cmd_gpio;
    uint8_t sdio_dat0_gpio;
    uint8_t card_detect_gpio;
} rad_sd_config_t;

typedef void (*rad_task_entry_t)(void *context);
typedef void (*rad_terminal_write_t)(const char *text, void *context);
typedef rad_status_t (*rad_terminal_handler_t)(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void *command_context);
typedef void (*rad_tty_output_t)(const void *data, size_t size, void *context);
typedef void (*rad_irq_handler_t)(uint32_t irq, void *context);
typedef void (*rad_work_handler_t)(void *context);

typedef struct rad_task_handle *rad_task_t;
typedef struct rad_mutex_handle *rad_mutex_t;
typedef struct rad_event_handle *rad_event_t;
typedef struct rad_wait_queue_handle *rad_wait_queue_t;
typedef struct rad_input_queue_handle *rad_input_queue_t;
typedef struct rad_file_handle *rad_file_t;
typedef struct rad_dir_handle *rad_dir_t;
typedef struct rad_device_handle *rad_device_t;
typedef struct rad_program_handle *rad_program_t;
typedef struct rad_tty_handle *rad_tty_t;
typedef struct rad_pty_handle *rad_pty_t;

typedef struct rad_tty_window_size {
    uint16_t rows;
    uint16_t columns;
} rad_tty_window_size_t;

rad_status_t rad_kernel_init(const rad_kernel_config_t *config);
void rad_kernel_shutdown(void);
int rad_kernel_is_initialized(void);
const char *rad_kernel_backend_name(void);
const char *rad_kernel_version_string(void);
rad_status_t rad_kernel_poll(void);
rad_status_t rad_kernel_run(void);
void rad_kernel_request_shutdown(void);
int rad_kernel_is_shutdown_requested(void);
int rad_vprintk(const char *format, va_list args);
int rad_printk(const char *format, ...);
int rad_early_vprintk(const char *format, va_list args);
int rad_early_printk(const char *format, ...);
void rad_debug_marker(const char *marker);
rad_status_t rad_cpu_interrupts_enable(void);
rad_status_t rad_cpu_interrupts_disable(void);
int rad_cpu_interrupts_enabled(void);
void rad_cpu_idle(void);
void rad_cpu_halt_forever(void);
rad_status_t rad_boot_info_set(const rad_boot_info_t *boot_info);
rad_status_t rad_boot_info_get(rad_boot_info_t *boot_info);
const char *rad_boot_arg_get(const char *key);

uint64_t rad_time_micros(void);
uint64_t rad_time_millis(void);
void rad_sleep_ms(uint32_t milliseconds);
void rad_sleep_us(uint32_t microseconds);
uint64_t rad_perf_now_cycles(void);
void rad_perf_counter_add(const char *name, uint64_t delta);
size_t rad_perf_counter_list(rad_perf_counter_info_t *counters, size_t capacity);
rad_status_t rad_work_submit(const char *name, rad_work_handler_t handler, void *context);
rad_status_t rad_work_poll(size_t budget, size_t *ran);
rad_status_t rad_wait_queue_create(rad_wait_queue_t *queue);
void rad_wait_queue_destroy(rad_wait_queue_t queue);
rad_status_t rad_wait_queue_wait(rad_wait_queue_t queue, uint32_t timeout_ms);
rad_status_t rad_wait_queue_wake_one(rad_wait_queue_t queue);
rad_status_t rad_wait_queue_wake_all(rad_wait_queue_t queue);
rad_status_t rad_timer_source_register(const rad_timer_source_config_t *config, const rad_timer_source_ops_t *ops);
rad_status_t rad_timer_source_unregister(const char *name);
void rad_timer_tick(uint64_t elapsed_micros);
rad_status_t rad_timer_schedule_oneshot(uint64_t delay_micros);
rad_status_t rad_timer_cancel_oneshot(void);
size_t rad_timer_list(rad_timer_source_info_t *timers, size_t capacity);

rad_status_t rad_task_create(rad_task_t *task, const char *name, rad_task_entry_t entry, void *context, size_t stack_size);
rad_status_t rad_task_create_config(rad_task_t *task, const rad_task_config_t *config, rad_task_entry_t entry, void *context);
rad_status_t rad_task_join(rad_task_t task);
void rad_task_detach(rad_task_t task);
uint64_t rad_task_current_id(void);
int rad_task_current_core(void);
void rad_task_yield(void);
void rad_task_sleep_ms(uint32_t milliseconds);
void rad_task_sleep_us(uint32_t microseconds);
size_t rad_task_list(rad_task_info_t *tasks, size_t capacity);
rad_status_t rad_core_info_get(rad_core_info_t *info);
rad_status_t rad_scheduler_info_get(rad_scheduler_info_t *info);
void rad_scheduler_yield_from_irq(void);
void rad_scheduler_set_preemption_enabled(int enabled);
uint32_t rad_scheduler_current_core(void);
uint32_t rad_scheduler_online_core_mask(void);

rad_status_t rad_mutex_create(rad_mutex_t *mutex);
void rad_mutex_destroy(rad_mutex_t mutex);
rad_status_t rad_mutex_lock(rad_mutex_t mutex);
rad_status_t rad_mutex_unlock(rad_mutex_t mutex);

rad_status_t rad_event_create(rad_event_t *event, int initially_signaled);
void rad_event_destroy(rad_event_t event);
rad_status_t rad_event_wait(rad_event_t event, uint32_t timeout_ms);
rad_status_t rad_event_signal(rad_event_t event);
rad_status_t rad_event_reset(rad_event_t event);

void *rad_memory_alloc(size_t size);
void rad_memory_free(void *pointer);
void rad_memory_get_stats(rad_memory_stats_t *stats);

rad_status_t rad_vfs_mount_host(const char *mount_point, const char *host_path);
rad_status_t rad_vfs_mount_sd(const rad_sd_config_t *config);
rad_status_t rad_vfs_mount_provider(const char *mount_point, const rad_vfs_backend_ops_t *ops);
rad_status_t rad_vfs_unmount(const char *mount_point);
rad_status_t rad_vfs_open(const char *path, uint32_t flags, rad_file_t *file);
rad_status_t rad_vfs_read(rad_file_t file, void *buffer, size_t size, size_t *bytes_read);
rad_status_t rad_vfs_write(rad_file_t file, const void *buffer, size_t size, size_t *bytes_written);
rad_status_t rad_vfs_seek(rad_file_t file, int64_t offset, rad_seek_origin_t origin);
uint64_t rad_vfs_tell(rad_file_t file);
void rad_vfs_close(rad_file_t file);
rad_status_t rad_vfs_stat(const char *path, rad_vfs_stat_t *stat);
rad_status_t rad_vfs_list(const char *path, rad_vfs_list_callback_t callback, void *context);
rad_status_t rad_vfs_mkdir(const char *path);
rad_status_t rad_vfs_remove(const char *path);
rad_status_t rad_vfs_rename(const char *old_path, const char *new_path);
rad_status_t rad_vfs_rmdir(const char *path);
rad_status_t rad_vfs_fsync(rad_file_t file);
rad_status_t rad_vfs_truncate(const char *path, uint64_t size);
rad_status_t rad_vfs_readlink(const char *path, char *buffer, size_t size);
rad_status_t rad_vfs_symlink(const char *target, const char *link_path);
rad_status_t rad_vfs_link(const char *old_path, const char *new_path);
rad_status_t rad_vfs_chmod(const char *path, uint32_t mode);
rad_status_t rad_vfs_chdir(const char *path);
rad_status_t rad_vfs_getcwd(char *buffer, size_t size);
rad_status_t rad_vfs_opendir(const char *path, rad_dir_t *dir);
rad_status_t rad_vfs_readdir(rad_dir_t dir, rad_vfs_dirent_t *entry);
void rad_vfs_closedir(rad_dir_t dir);

int32_t rad_process_current_pid(void);
int32_t rad_process_parent_pid(void);
int32_t rad_process_fork(void);
int32_t rad_process_execve(const char *path, const char *const argv[]);
int32_t rad_process_waitpid(int32_t pid, int32_t *status, uint32_t options);
void rad_process_exit(int32_t status);
size_t rad_process_list(rad_process_info_t *processes, size_t capacity);
int32_t rad_process_create(const char *path, int32_t parent_pid);
rad_status_t rad_process_attach_task(int32_t pid, rad_task_t task);
void rad_process_set_current_pid(int32_t pid);
rad_status_t rad_process_mark_exec(int32_t pid, const char *path);
rad_status_t rad_process_clone_fds(int32_t parent_pid, int32_t child_pid);
rad_status_t rad_process_arch_register(const rad_process_arch_ops_t *ops);
int32_t rad_process_fork_from_arch_frame(void *trap_frame);
int32_t rad_process_reap(int32_t pid, int32_t *status);

rad_status_t rad_module_register(const rad_module_descriptor_t *descriptor);
rad_status_t rad_module_unregister(const char *name);
size_t rad_module_list(rad_module_info_t *modules, size_t capacity);

rad_status_t rad_irq_register(uint32_t irq, const char *name, rad_irq_handler_t handler, void *context);
rad_status_t rad_irq_unregister(uint32_t irq);
rad_status_t rad_irq_enable(uint32_t irq);
rad_status_t rad_irq_disable(uint32_t irq);
rad_status_t rad_irq_dispatch(uint32_t irq);
size_t rad_irq_list(rad_irq_info_t *irqs, size_t capacity);
rad_status_t rad_irq_domain_register(const rad_irq_domain_config_t *config, rad_irq_domain_t *domain);
rad_status_t rad_irq_domain_unregister(const char *tree_path);
rad_status_t rad_irq_domain_find(const char *tree_path, rad_irq_domain_t *domain);
size_t rad_irq_resolve_tree(rad_tree_node_t node, rad_irq_resource_t *resources, size_t capacity);
rad_status_t rad_irq_resource_enable(const rad_irq_resource_t *resource);
rad_status_t rad_irq_resource_disable(const rad_irq_resource_t *resource);
rad_status_t rad_irq_resource_register_handler(const rad_irq_resource_t *resource, const char *name, rad_irq_handler_t handler, void *context);

int32_t rad_fd_open(const char *path, uint32_t flags);
int32_t rad_fd_close(int32_t fd);
intptr_t rad_fd_read(int32_t fd, void *buffer, size_t size);
intptr_t rad_fd_write(int32_t fd, const void *buffer, size_t size);
int64_t rad_fd_lseek(int32_t fd, int64_t offset, rad_seek_origin_t origin);
int32_t rad_fd_ioctl(int32_t fd, uint32_t request, void *argument);
int32_t rad_fd_stat(const char *path, rad_vfs_stat_t *stat);
int32_t rad_fd_fstat(int32_t fd, rad_vfs_stat_t *stat);
int32_t rad_fd_dup(int32_t fd);
int32_t rad_fd_dup2(int32_t old_fd, int32_t new_fd);
int32_t rad_fd_fcntl(int32_t fd, uint32_t command, uintptr_t argument);
int32_t rad_pipe_create(int32_t pipefd[2]);

intptr_t rad_syscall_dispatch(uintptr_t number, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);

rad_status_t rad_program_load(const char *path, rad_program_t *program);
rad_status_t rad_program_spawn(rad_program_t program, int argc, const char **argv, rad_task_t *task);
void rad_program_unload(rad_program_t program);
size_t rad_program_list(rad_program_info_t *programs, size_t capacity);

rad_status_t rad_overlay_load_memory(const void *data, size_t size);
rad_status_t rad_overlay_load_file(const char *path);
rad_status_t rad_overlay_apply_boot(void);
size_t rad_overlay_list(rad_overlay_info_t *overlays, size_t capacity);
size_t rad_tree_list(rad_tree_node_info_t *nodes, size_t capacity);
rad_tree_node_t rad_tree_find(const char *path);
rad_status_t rad_tree_get_property_u32(rad_tree_node_t node, const char *name, uint32_t *value);
size_t rad_tree_get_property_u32_array(rad_tree_node_t node, const char *name, uint32_t *values, size_t capacity);
rad_status_t rad_tree_get_property_bool(rad_tree_node_t node, const char *name, int *value);
rad_status_t rad_tree_get_property_string(rad_tree_node_t node, const char *name, char *buffer, size_t size);

rad_status_t rad_device_register(const char *name, rad_device_type_t type, const rad_device_ops_t *ops);
rad_status_t rad_device_unregister(const char *name);
rad_status_t rad_device_open(const char *name, rad_device_t *device);
void rad_device_close(rad_device_t device);
rad_status_t rad_device_read(rad_device_t device, void *buffer, size_t size, size_t *bytes_read);
rad_status_t rad_device_write(rad_device_t device, const void *buffer, size_t size, size_t *bytes_written);
rad_status_t rad_device_ioctl(rad_device_t device, uint32_t request, void *argument);
size_t rad_device_list(char names[][64], rad_device_type_t *types, size_t capacity);

rad_status_t rad_input_device_register(const char *name, const rad_device_ops_t *ops);
rad_status_t rad_input_device_unregister(const char *name);
rad_status_t rad_input_open(const char *name, rad_device_t *device);
rad_status_t rad_input_read_event(rad_device_t device, rad_input_event_t *event);
rad_status_t rad_input_queue_create(const char *name, size_t capacity, rad_input_queue_t *queue);
void rad_input_queue_destroy(rad_input_queue_t queue);
rad_status_t rad_input_queue_push(rad_input_queue_t queue, const rad_input_event_t *event);
rad_status_t rad_input_queue_read(rad_input_queue_t queue, rad_input_event_t *events, size_t capacity, uint32_t timeout_ms, size_t *count);
rad_status_t rad_input_device_register_queue(const char *name, rad_input_queue_t queue);

rad_status_t rad_block_device_register(const char *name, const rad_device_ops_t *ops);
rad_status_t rad_block_open(const char *name, rad_device_t *device);
rad_status_t rad_block_info(rad_device_t device, rad_block_info_t *info);
rad_status_t rad_block_read(rad_device_t device, uint64_t sector, uint32_t sector_count, void *buffer);
rad_status_t rad_block_write(rad_device_t device, uint64_t sector, uint32_t sector_count, const void *buffer);
rad_status_t rad_block_flush(rad_device_t device);
rad_status_t rad_net_device_register(const char *name, const rad_device_ops_t *ops);
rad_status_t rad_net_open(const char *name, rad_device_t *device);
rad_status_t rad_net_link_info(rad_device_t device, rad_net_link_info_t *info);
rad_status_t rad_net_send(rad_device_t device, const void *data, size_t length);
rad_status_t rad_net_poll(rad_device_t device);

rad_status_t rad_framebuffer_register(const char *name, const rad_framebuffer_info_t *info, const rad_framebuffer_ops_t *ops);
rad_status_t rad_framebuffer_register_ex(const rad_framebuffer_config_t *config, const rad_framebuffer_ops_t *ops);
rad_status_t rad_framebuffer_unregister(const char *name);
rad_status_t rad_framebuffer_open(const char *name, rad_framebuffer_t *framebuffer);
rad_status_t rad_framebuffer_open_primary(rad_framebuffer_t *framebuffer);
void rad_framebuffer_close(rad_framebuffer_t framebuffer);
rad_status_t rad_framebuffer_get_info(rad_framebuffer_t framebuffer, rad_framebuffer_info_t *info);
rad_status_t rad_framebuffer_get_display_info(rad_framebuffer_t framebuffer, rad_framebuffer_display_info_t *info);
rad_status_t rad_framebuffer_set_primary(const char *name);
rad_status_t rad_framebuffer_set_mode(rad_framebuffer_t framebuffer, uint32_t mode_index);
rad_status_t rad_framebuffer_blank(rad_framebuffer_t framebuffer, int blanked);
rad_status_t rad_framebuffer_get_vsync_counter(rad_framebuffer_t framebuffer, uint64_t *counter);
rad_status_t rad_framebuffer_clear(rad_framebuffer_t framebuffer, uint32_t color);
rad_status_t rad_framebuffer_draw_text(rad_framebuffer_t framebuffer, uint32_t cell_x, uint32_t cell_y, const char *text, uint32_t foreground, uint32_t background);
rad_status_t rad_framebuffer_flush(rad_framebuffer_t framebuffer, const rad_framebuffer_rect_t *rect);
size_t rad_framebuffer_list(rad_framebuffer_info_t *framebuffers, char names[][64], size_t capacity);
size_t rad_framebuffer_list_ex(rad_framebuffer_display_info_t *framebuffers, size_t capacity);

rad_status_t rad_i2c_controller_register(const rad_i2c_controller_config_t *config, const rad_i2c_controller_ops_t *ops);
rad_status_t rad_i2c_controller_unregister(uint32_t bus_id);
rad_status_t rad_i2c_bus_open(uint32_t bus_id, rad_i2c_bus_t *bus);
void rad_i2c_bus_close(rad_i2c_bus_t bus);
rad_status_t rad_i2c_transfer(rad_i2c_bus_t bus, const rad_i2c_transfer_t *transfer);
rad_status_t rad_i2c_bind_tree(void);
rad_status_t rad_i2c_driver_register(const rad_i2c_driver_t *driver);
rad_status_t rad_i2c_driver_unregister(const char *name);
rad_status_t rad_i2c_device_open(uint32_t bus_id, uint8_t address, rad_i2c_device_t **device);
void rad_i2c_device_close(rad_i2c_device_t *device);
rad_status_t rad_i2c_device_transfer(rad_i2c_device_t *device, const rad_i2c_transfer_t *transfer);
size_t rad_i2c_device_irq_count(const rad_i2c_device_t *device);
rad_status_t rad_i2c_device_get_irq(const rad_i2c_device_t *device, size_t index, rad_irq_resource_t *resource);
size_t rad_i2c_list_devices(rad_i2c_device_info_t *devices, size_t capacity);
rad_status_t rad_i2c_transfer_device(rad_device_t device, const rad_i2c_transfer_t *transfer);

rad_status_t rad_spi_controller_register(const rad_spi_controller_config_t *config, const rad_spi_controller_ops_t *ops);
rad_status_t rad_spi_controller_unregister(uint32_t bus_id);
rad_status_t rad_spi_bus_open(uint32_t bus_id, rad_spi_bus_t *bus);
void rad_spi_bus_close(rad_spi_bus_t bus);
rad_status_t rad_spi_transfer(rad_spi_bus_t bus, const rad_spi_transfer_t *transfer);
rad_status_t rad_spi_driver_register(const rad_spi_driver_t *driver);
rad_status_t rad_spi_driver_unregister(const char *name);
rad_status_t rad_spi_device_open(uint32_t bus_id, uint8_t cs, rad_spi_device_t **device);
void rad_spi_device_close(rad_spi_device_t *device);
rad_status_t rad_spi_device_transfer(rad_spi_device_t *device, const rad_spi_transfer_t *transfer);
size_t rad_spi_device_irq_count(const rad_spi_device_t *device);
rad_status_t rad_spi_device_get_irq(const rad_spi_device_t *device, size_t index, rad_irq_resource_t *resource);
size_t rad_spi_list_devices(rad_spi_device_info_t *devices, size_t capacity);
rad_status_t rad_spi_transfer_device(rad_device_t device, const rad_spi_transfer_t *transfer);

rad_status_t rad_dma_controller_register(const rad_dma_controller_config_t *config, const rad_dma_backend_ops_t *ops);
rad_status_t rad_dma_controller_unregister(const char *name);
rad_status_t rad_dma_channel_request(rad_dma_request_id_t request_id, rad_dma_channel_t *channel);
void rad_dma_channel_release(rad_dma_channel_t channel);
rad_status_t rad_dma_submit(rad_dma_channel_t channel, const rad_dma_transfer_t *transfer);
rad_status_t rad_dma_wait(rad_dma_channel_t channel, uint32_t timeout_ms);
rad_status_t rad_dma_cancel(rad_dma_channel_t channel);
size_t rad_dma_list_channels(rad_dma_channel_info_t *channels, size_t capacity);

size_t rad_driver_list(rad_driver_info_t *drivers, size_t capacity);
rad_status_t rad_audio_configure(rad_device_t device, const rad_audio_format_t *format);
rad_status_t rad_serial_configure(rad_device_t device, const rad_serial_config_t *config);

rad_status_t rad_tty_open(const char *name, rad_tty_t *tty);
void rad_tty_close(rad_tty_t tty);
rad_status_t rad_tty_read(rad_tty_t tty, void *buffer, size_t size, size_t *bytes_read);
rad_status_t rad_tty_write(rad_tty_t tty, const void *buffer, size_t size, size_t *bytes_written);
rad_status_t rad_tty_push_input(rad_tty_t tty, const void *buffer, size_t size, size_t *bytes_consumed);
rad_status_t rad_tty_set_output_callback(rad_tty_t tty, rad_tty_output_t output, void *context);
rad_status_t rad_tty_set_window_size(rad_tty_t tty, const rad_tty_window_size_t *size);
rad_status_t rad_tty_get_window_size(rad_tty_t tty, rad_tty_window_size_t *size);
rad_status_t rad_tty_set_mode(rad_tty_t tty, uint32_t mode);
rad_status_t rad_tty_get_mode(rad_tty_t tty, uint32_t *mode);

rad_status_t rad_pty_open_pair(const char *name, rad_pty_t *pty);
void rad_pty_close(rad_pty_t pty);
rad_status_t rad_pty_read_master(rad_pty_t pty, void *buffer, size_t size, size_t *bytes_read);
rad_status_t rad_pty_write_master(rad_pty_t pty, const void *buffer, size_t size, size_t *bytes_written);
rad_status_t rad_pty_read_slave(rad_pty_t pty, void *buffer, size_t size, size_t *bytes_read);
rad_status_t rad_pty_write_slave(rad_pty_t pty, const void *buffer, size_t size, size_t *bytes_written);
rad_status_t rad_pty_resize(rad_pty_t pty, const rad_tty_window_size_t *size);
rad_status_t rad_pty_slave_name(rad_pty_t pty, char *buffer, size_t size);

rad_status_t rad_terminal_register_command(const char *name, const char *description, rad_terminal_handler_t handler, void *context);
rad_status_t rad_terminal_execute(const char *line, rad_terminal_write_t write, void *write_context);
rad_status_t rad_terminal_poll_tty(rad_tty_t tty);
rad_status_t rad_terminal_attach_device(const char *device_name);
rad_status_t rad_terminal_poll_attached(void);
size_t rad_terminal_command_count(void);

#if defined(__cplusplus)
}

#ifndef RADLIB_EMBEDDED_NO_CPP_WRAPPERS
namespace RADEmbedded {

class Kernel {
public:
    static rad_status_t init(const char *backend = "linux_sim") {
        rad_kernel_config_t config{};
        config.backend_name = backend;
        return rad_kernel_init(&config);
    }
    static void shutdown() { rad_kernel_shutdown(); }
    static bool initialized() { return rad_kernel_is_initialized() != 0; }
    static std::string backendName() { return rad_kernel_backend_name(); }
    static std::string version() { return rad_kernel_version_string(); }
};

class Mutex {
public:
    Mutex() { rad_mutex_create(&handle_); }
    ~Mutex() { rad_mutex_destroy(handle_); }
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    rad_status_t lock() { return rad_mutex_lock(handle_); }
    rad_status_t unlock() { return rad_mutex_unlock(handle_); }
    rad_mutex_t native() const { return handle_; }
private:
    rad_mutex_t handle_ = nullptr;
};

class Event {
public:
    explicit Event(bool signaled = false) { rad_event_create(&handle_, signaled ? 1 : 0); }
    ~Event() { rad_event_destroy(handle_); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    rad_status_t wait(uint32_t timeoutMs) { return rad_event_wait(handle_, timeoutMs); }
    rad_status_t signal() { return rad_event_signal(handle_); }
    rad_status_t reset() { return rad_event_reset(handle_); }
private:
    rad_event_t handle_ = nullptr;
};

class File {
public:
    File() = default;
    ~File() { close(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    bool open(const char *path, uint32_t flags) { close(); return rad_vfs_open(path, flags, &handle_) == RAD_STATUS_OK; }
    size_t read(void *buffer, size_t size) { size_t done = 0; rad_vfs_read(handle_, buffer, size, &done); return done; }
    size_t write(const void *buffer, size_t size) { size_t done = 0; rad_vfs_write(handle_, buffer, size, &done); return done; }
    void close() { if (handle_) { rad_vfs_close(handle_); handle_ = nullptr; } }
private:
    rad_file_t handle_ = nullptr;
};

} // namespace RADEmbedded
#endif
#endif

#endif
