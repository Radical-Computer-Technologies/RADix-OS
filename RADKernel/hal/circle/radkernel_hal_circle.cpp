#include <radkernel/radkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if __has_include(<circle/timer.h>)
#include <circle/timer.h>
#include <circle/serial.h>
#include <circle/screen.h>
#include <circle/memory.h>
#include <circle/multicore.h>
#define RAD_KERNEL_HAS_CIRCLE 1
#else
#define RAD_KERNEL_HAS_CIRCLE 0
#endif

namespace {
#if RAD_KERNEL_HAS_CIRCLE
CSerialDevice *g_serial = nullptr;
CTimer *g_timer = nullptr;
CScreenDevice *g_screen = nullptr;
#endif

uint16_t g_framebuffer[320u * 240u];

#if RAD_KERNEL_HAS_CIRCLE && defined(ARM_ALLOW_MULTI_CORE)
void (*g_worker_entry)(uint32_t core) = nullptr;
uint32_t g_started_mask = 0;

class RadCircleMultiCore final : public CMultiCoreSupport {
public:
    RadCircleMultiCore() : CMultiCoreSupport(CMemorySystem::Get()) {}
    void Run(unsigned nCore) override {
        if (nCore > 0 && g_worker_entry) g_worker_entry(nCore);
    }
};

RadCircleMultiCore *g_multicore = nullptr;
#endif

rad_status_t circle_serial_read(void*, void *buffer, size_t size, size_t *bytes_read) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    size_t count = 0;
#if RAD_KERNEL_HAS_CIRCLE
    if (g_serial) {
        int result = g_serial->Read(buffer, size);
        if (result > 0) count = static_cast<size_t>(result);
    }
#else
    (void)size;
#endif
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t circle_serial_write(void*, const void *buffer, size_t size, size_t *bytes_written) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
#if RAD_KERNEL_HAS_CIRCLE
    if (g_serial) g_serial->Write(buffer, size);
#else
    (void)buffer;
#endif
    if (bytes_written) *bytes_written = size;
    return RAD_STATUS_OK;
}

rad_status_t circle_serial_ioctl(void*, uint32_t request, void *argument) {
    if (request != RAD_DEVICE_IOCTL_SERIAL_CONFIGURE) return RAD_STATUS_NOT_SUPPORTED;
    return argument ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
}

rad_status_t circle_i2c_transfer(void*, const rad_i2c_transfer_t*) {
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t circle_spi_transfer(void*, const rad_spi_device_t*, const rad_spi_transfer_t*) {
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t circle_spi_transfer_dma(void*, const rad_spi_device_t*, const rad_spi_transfer_t*, rad_dma_channel_t, rad_dma_channel_t) {
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t circle_framebuffer_flush(void*, const rad_framebuffer_rect_t *rect) {
#if RAD_KERNEL_HAS_CIRCLE
    if (g_screen) {
        const uint32_t x0 = rect ? rect->x : 0u;
        const uint32_t y0 = rect ? rect->y : 0u;
        const uint32_t x1 = rect ? (rect->x + rect->width) : 320u;
        const uint32_t y1 = rect ? (rect->y + rect->height) : 240u;
        const uint32_t max_x = x1 > 320u ? 320u : x1;
        const uint32_t max_y = y1 > 240u ? 240u : y1;
        for (uint32_t y = y0; y < max_y; ++y) {
            for (uint32_t x = x0; x < max_x; ++x) {
                g_screen->SetPixel(x, y, g_framebuffer[y * 320u + x]);
            }
        }
        g_screen->Update(0);
    }
#else
    (void)rect;
#endif
    return RAD_STATUS_OK;
}

void register_alias(const char *name, const rad_device_ops_t *ops) {
    rad_device_register(name, RAD_DEVICE_SERIAL, ops);
}
}

extern "C" void rad_circle_bind_platform(void *serial_device, void *timer) {
#if RAD_KERNEL_HAS_CIRCLE
    g_serial = static_cast<CSerialDevice*>(serial_device);
    g_timer = static_cast<CTimer*>(timer);
#else
    (void)serial_device;
    (void)timer;
#endif
}

extern "C" void rad_circle_bind_platform_ex(void *serial_device, void *timer, void *screen_device) {
#if RAD_KERNEL_HAS_CIRCLE
    g_serial = static_cast<CSerialDevice*>(serial_device);
    g_timer = static_cast<CTimer*>(timer);
    g_screen = static_cast<CScreenDevice*>(screen_device);
#else
    (void)serial_device;
    (void)timer;
    (void)screen_device;
#endif
}

extern "C" uint64_t rad_hal_time_micros(void) {
#if RAD_KERNEL_HAS_CIRCLE
    return CTimer::GetClockTicks64();
#else
    return 0;
#endif
}

extern "C" void rad_hal_sleep_us(uint32_t microseconds) {
#if RAD_KERNEL_HAS_CIRCLE
    if (g_timer) {
        g_timer->usDelay(microseconds);
    }
#else
    (void)microseconds;
#endif
}

extern "C" uint32_t rad_hal_core_count(void) {
#if RAD_KERNEL_HAS_CIRCLE && defined(ARM_ALLOW_MULTI_CORE)
    return CORES;
#else
    return 1u;
#endif
}

extern "C" uint32_t rad_hal_current_core(void) {
#if RAD_KERNEL_HAS_CIRCLE && defined(ARM_ALLOW_MULTI_CORE)
    return CMultiCoreSupport::ThisCore();
#else
    return 0u;
#endif
}

extern "C" rad_status_t rad_hal_start_worker_core(uint32_t core, void (*entry)(uint32_t core)) {
#if RAD_KERNEL_HAS_CIRCLE && defined(ARM_ALLOW_MULTI_CORE)
    if (!entry || core == 0 || core >= CORES) return RAD_STATUS_NOT_SUPPORTED;
    g_worker_entry = entry;
    if (!g_multicore) {
        g_multicore = new RadCircleMultiCore();
        if (!g_multicore) return RAD_STATUS_NO_MEMORY;
        if (!g_multicore->Initialize()) return RAD_STATUS_ERROR;
        for (uint32_t n = 1; n < CORES; ++n) g_started_mask |= (1u << n);
    }
    return (g_started_mask & (1u << core)) ? RAD_STATUS_OK : RAD_STATUS_NOT_SUPPORTED;
#else
    (void)core;
    (void)entry;
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}

extern "C" void rad_hal_worker_wait(void) {
#if RAD_KERNEL_HAS_CIRCLE
    if (g_timer) g_timer->usDelay(1000);
#endif
}

extern "C" void rad_hal_worker_wake(void) {
}

extern "C" rad_status_t rad_hal_register_default_devices(void) {
    rad_device_ops_t serial{};
    serial.read = circle_serial_read;
    serial.write = circle_serial_write;
    serial.ioctl = circle_serial_ioctl;
    register_alias("/dev/console", &serial);
    register_alias("/dev/serial0", &serial);
    register_alias("/dev/uart0", &serial);
    register_alias("/dev/ttyS0", &serial);
    rad_i2c_controller_config_t i2c_config{};
    i2c_config.size = sizeof(i2c_config);
    i2c_config.bus_id = 0;
    i2c_config.name = "rad_i2c_bcm283x";
    i2c_config.tree_path = "/soc/i2c@0";
    i2c_config.clock_hz = 100000u;
    rad_i2c_controller_ops_t i2c_ops{};
    i2c_ops.transfer = circle_i2c_transfer;
    rad_i2c_controller_register(&i2c_config, &i2c_ops);
    rad_spi_controller_config_t spi_config{};
    spi_config.size = sizeof(spi_config);
    spi_config.bus_id = 0;
    spi_config.name = "rad_spi_bcm283x";
    spi_config.tree_path = "/soc/spi@0";
    spi_config.clock_hz = 1000000u;
    rad_spi_controller_ops_t spi_ops{};
    spi_ops.transfer = circle_spi_transfer;
    spi_ops.transfer_dma = circle_spi_transfer_dma;
    rad_spi_controller_register(&spi_config, &spi_ops);
    rad_dma_controller_config_t dma_config{};
    dma_config.size = sizeof(dma_config);
    dma_config.name = "rad_dma_bcm283x";
    dma_config.channel_count = 0;
    rad_dma_backend_ops_t dma_ops{};
    rad_dma_controller_register(&dma_config, &dma_ops);
    rad_framebuffer_config_t fb_config{};
    fb_config.size = sizeof(fb_config);
    fb_config.name = "/dev/fb0";
    fb_config.info.size = sizeof(fb_config.info);
    fb_config.info.width = 320u;
    fb_config.info.height = 240u;
    fb_config.info.stride_bytes = 320u * 2u;
    fb_config.info.pixel_format = RAD_PIXEL_FORMAT_RGB565;
    fb_config.info.pixels = g_framebuffer;
    fb_config.output_type = RAD_DISPLAY_OUTPUT_CIRCLE;
    fb_config.connector = "circle-screen";
    fb_config.mode_count = 1u;
    fb_config.preferred_mode = 0u;
    fb_config.modes[0].width = fb_config.info.width;
    fb_config.modes[0].height = fb_config.info.height;
    fb_config.modes[0].refresh_hz = 60u;
    fb_config.modes[0].stride_bytes = fb_config.info.stride_bytes;
    fb_config.modes[0].pixel_format = fb_config.info.pixel_format;
    fb_config.primary = 1;
    rad_framebuffer_ops_t fb_ops{};
    fb_ops.flush = circle_framebuffer_flush;
    rad_framebuffer_register_ex(&fb_config, &fb_ops);
    rad_terminal_attach_device("/dev/console");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_mount_sd(const rad_sd_config_t *config) {
    if (!config) return RAD_STATUS_INVALID_ARGUMENT;
    return RAD_STATUS_NOT_SUPPORTED;
}
