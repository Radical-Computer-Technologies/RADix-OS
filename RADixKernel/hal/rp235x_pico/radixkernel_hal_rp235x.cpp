#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if __has_include("pico/stdlib.h")
#include "pico/stdlib.h"
#include "pico/platform.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#if __has_include("hardware/dma.h") && __has_include("hardware/regs/dreq.h")
#include "hardware/dma.h"
#include "hardware/regs/dreq.h"
#define RADIX_KERNEL_HAS_PICO_DMA 1
#else
#define RADIX_KERNEL_HAS_PICO_DMA 0
#endif
#define RADIX_KERNEL_HAS_PICO_SDK 1
#else
#define RADIX_KERNEL_HAS_PICO_SDK 0
#define RADIX_KERNEL_HAS_PICO_DMA 0
#endif

namespace {
#if RADIX_KERNEL_HAS_PICO_SDK
void (*g_worker_entry)(uint32_t core) = nullptr;
bool g_core1_started = false;

void core1_entry() {
    if (g_worker_entry) g_worker_entry(1u);
}
#endif

uint16_t g_framebuffer[160u * 120u];

#if RADIX_KERNEL_HAS_PICO_DMA
struct PicoDmaChannel {
    int channel = -1;
    rad_dma_request_id_t request_id = RAD_DMA_DREQ_NONE;
    int active = 0;
};

PicoDmaChannel g_dma_channels[12];

uint pico_dma_dreq(rad_dma_request_id_t request_id) {
    switch (request_id) {
        case RAD_DMA_DREQ_NONE: return DREQ_FORCE;
        case RAD_DMA_DREQ_SPI0_TX: return DREQ_SPI0_TX;
        case RAD_DMA_DREQ_SPI0_RX: return DREQ_SPI0_RX;
        case RAD_DMA_DREQ_SPI1_TX: return DREQ_SPI1_TX;
        case RAD_DMA_DREQ_SPI1_RX: return DREQ_SPI1_RX;
        case RAD_DMA_DREQ_RP2350_HSTX_TX: return DREQ_HSTX;
        default: return DREQ_FORCE;
    }
}

#endif

rad_status_t pico_serial_read(void*, void *buffer, size_t size, size_t *bytes_read) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    size_t count = 0;
#if RADIX_KERNEL_HAS_PICO_SDK
    while (count < size) {
        const int ch = getchar_timeout_us(0);
        if (ch < 0) break;
        static_cast<char*>(buffer)[count++] = static_cast<char>(ch);
    }
#else
    (void)size;
#endif
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t pico_serial_write(void*, const void *buffer, size_t size, size_t *bytes_written) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
#if RADIX_KERNEL_HAS_PICO_SDK
    const char *text = static_cast<const char*>(buffer);
    for (size_t i = 0; i < size; ++i) putchar_raw(text[i]);
#else
    (void)buffer;
#endif
    if (bytes_written) *bytes_written = size;
    return RAD_STATUS_OK;
}

rad_status_t pico_serial_ioctl(void*, uint32_t request, void *argument) {
    if (request != RAD_DEVICE_IOCTL_SERIAL_CONFIGURE) return RAD_STATUS_NOT_SUPPORTED;
    if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
    return RAD_STATUS_OK;
}

rad_status_t pico_i2c_transfer(void*, const rad_i2c_transfer_t *transfer) {
    if (!transfer) return RAD_STATUS_INVALID_ARGUMENT;
#if RADIX_KERNEL_HAS_PICO_SDK
    int result = 0;
    if (transfer->write_data && transfer->write_size) {
        result = i2c_write_blocking(i2c0, transfer->address, transfer->write_data, transfer->write_size, transfer->read_size > 0);
        if (result < 0 || static_cast<size_t>(result) != transfer->write_size) return RAD_STATUS_ERROR;
    }
    if (transfer->read_data && transfer->read_size) {
        result = i2c_read_blocking(i2c0, transfer->address, transfer->read_data, transfer->read_size, false);
        if (result < 0 || static_cast<size_t>(result) != transfer->read_size) return RAD_STATUS_ERROR;
    }
    return RAD_STATUS_OK;
#else
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}

rad_status_t pico_spi_transfer(void*, const rad_spi_device_t*, const rad_spi_transfer_t *transfer) {
    if (!transfer || transfer->size == 0) return RAD_STATUS_INVALID_ARGUMENT;
#if RADIX_KERNEL_HAS_PICO_SDK
    spi_hw_t *hw = spi_get_hw(spi0);
    for (size_t i = 0; i < transfer->size; ++i) {
        while ((hw->sr & SPI_SSPSR_TNF_BITS) == 0) tight_loop_contents();
        hw->dr = transfer->tx_data ? transfer->tx_data[i] : 0xffu;
        while ((hw->sr & SPI_SSPSR_RNE_BITS) == 0) tight_loop_contents();
        const uint8_t value = static_cast<uint8_t>(hw->dr & 0xffu);
        if (transfer->rx_data) transfer->rx_data[i] = value;
    }
    return RAD_STATUS_OK;
#else
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}

rad_status_t pico_spi_transfer_dma(void*, const rad_spi_device_t*, const rad_spi_transfer_t *transfer, rad_dma_channel_t tx_channel, rad_dma_channel_t rx_channel) {
    if (!transfer || transfer->size == 0 || !tx_channel || !rx_channel) return RAD_STATUS_INVALID_ARGUMENT;
#if RADIX_KERNEL_HAS_PICO_SDK
    spi_hw_t *hw = spi_get_hw(spi0);
    rad_dma_transfer_t tx{};
    tx.size = static_cast<uint32_t>(transfer->size);
    tx.type = RAD_DMA_MEMORY_TO_DEVICE;
    tx.request_id = RAD_DMA_DREQ_SPI0_TX;
    tx.source = transfer->tx_data;
    tx.peripheral_address = reinterpret_cast<uintptr_t>(&hw->dr);
    rad_dma_transfer_t rx{};
    rx.size = static_cast<uint32_t>(transfer->size);
    rx.type = RAD_DMA_DEVICE_TO_MEMORY;
    rx.request_id = RAD_DMA_DREQ_SPI0_RX;
    rx.destination = transfer->rx_data;
    rx.peripheral_address = reinterpret_cast<uintptr_t>(&hw->dr);
    rad_status_t status = rad_dma_submit(rx_channel, &rx);
    if (status == RAD_STATUS_OK) status = rad_dma_submit(tx_channel, &tx);
    if (status == RAD_STATUS_OK) status = rad_dma_wait(tx_channel, 1000);
    if (status == RAD_STATUS_OK) status = rad_dma_wait(rx_channel, 1000);
    return status;
#else
    (void)device;
    (void)transfer;
    (void)tx_channel;
    (void)rx_channel;
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}

rad_status_t pico_framebuffer_flush(void*, const rad_framebuffer_rect_t*) {
    return RAD_STATUS_OK;
}

void register_alias(const char *name, const rad_device_ops_t *ops) {
    rad_device_register(name, RAD_DEVICE_SERIAL, ops);
}

rad_status_t pico_dma_request(void*, rad_dma_request_id_t request_id, void **backend_channel) {
    if (!backend_channel) return RAD_STATUS_INVALID_ARGUMENT;
#if RADIX_KERNEL_HAS_PICO_DMA
    const int channel = dma_claim_unused_channel(false);
    if (channel < 0) return RAD_STATUS_NOT_SUPPORTED;
    for (auto& slot : g_dma_channels) {
        if (slot.channel >= 0) continue;
        slot.channel = channel;
        slot.request_id = request_id;
        slot.active = 0;
        *backend_channel = &slot;
        return RAD_STATUS_OK;
    }
    dma_channel_unclaim(static_cast<uint>(channel));
    return RAD_STATUS_NO_MEMORY;
#else
    (void)request_id;
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}

void pico_dma_release(void*, void *backend_channel) {
#if RADIX_KERNEL_HAS_PICO_DMA
    auto *slot = static_cast<PicoDmaChannel*>(backend_channel);
    if (!slot || slot->channel < 0) return;
    if (slot->active) dma_channel_abort(static_cast<uint>(slot->channel));
    dma_channel_unclaim(static_cast<uint>(slot->channel));
    slot->channel = -1;
    slot->request_id = RAD_DMA_DREQ_NONE;
    slot->active = 0;
#else
    (void)backend_channel;
#endif
}

rad_status_t pico_dma_submit(void*, void *backend_channel, const rad_dma_transfer_t *transfer) {
    if (!backend_channel || !transfer || transfer->size == 0) return RAD_STATUS_INVALID_ARGUMENT;
#if RADIX_KERNEL_HAS_PICO_DMA
    auto *slot = static_cast<PicoDmaChannel*>(backend_channel);
    if (!slot || slot->channel < 0) return RAD_STATUS_INVALID_ARGUMENT;
    const void *read_addr = transfer->source;
    void *write_addr = transfer->destination;
    bool read_increment = true;
    bool write_increment = true;
    if (transfer->type == RAD_DMA_MEMORY_TO_DEVICE) {
        if (!transfer->source || transfer->peripheral_address == 0) return RAD_STATUS_INVALID_ARGUMENT;
        read_addr = transfer->source;
        write_addr = reinterpret_cast<void*>(transfer->peripheral_address);
        write_increment = false;
    } else if (transfer->type == RAD_DMA_DEVICE_TO_MEMORY) {
        if (!transfer->destination || transfer->peripheral_address == 0) return RAD_STATUS_INVALID_ARGUMENT;
        read_addr = reinterpret_cast<const void*>(transfer->peripheral_address);
        write_addr = transfer->destination;
        read_increment = false;
    } else if (!transfer->source || !transfer->destination) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    dma_channel_config cfg = dma_channel_get_default_config(static_cast<uint>(slot->channel));
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, read_increment);
    channel_config_set_write_increment(&cfg, write_increment);
    channel_config_set_dreq(&cfg, pico_dma_dreq(transfer->request_id));
    dma_channel_configure(static_cast<uint>(slot->channel), &cfg, write_addr, read_addr, transfer->size, true);
    slot->active = 1;
    return RAD_STATUS_OK;
#else
    (void)backend_channel;
    (void)transfer;
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}

rad_status_t pico_dma_wait(void*, void *backend_channel, uint32_t timeout_ms) {
    if (!backend_channel) return RAD_STATUS_INVALID_ARGUMENT;
#if RADIX_KERNEL_HAS_PICO_DMA
    auto *slot = static_cast<PicoDmaChannel*>(backend_channel);
    if (!slot || slot->channel < 0) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t deadline = time_us_64() + static_cast<uint64_t>(timeout_ms) * 1000u;
    while (dma_channel_is_busy(static_cast<uint>(slot->channel))) {
        if (timeout_ms != 0 && time_us_64() >= deadline) return RAD_STATUS_TIMEOUT;
        tight_loop_contents();
    }
    slot->active = 0;
    return RAD_STATUS_OK;
#else
    (void)timeout_ms;
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}

rad_status_t pico_dma_cancel(void*, void *backend_channel) {
    if (!backend_channel) return RAD_STATUS_INVALID_ARGUMENT;
#if RADIX_KERNEL_HAS_PICO_DMA
    auto *slot = static_cast<PicoDmaChannel*>(backend_channel);
    if (!slot || slot->channel < 0) return RAD_STATUS_INVALID_ARGUMENT;
    dma_channel_abort(static_cast<uint>(slot->channel));
    slot->active = 0;
    return RAD_STATUS_OK;
#else
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}
}

extern "C" uint64_t rad_hal_time_micros(void) {
#if RADIX_KERNEL_HAS_PICO_SDK
    return time_us_64();
#else
    return 0;
#endif
}

extern "C" void rad_hal_sleep_us(uint32_t microseconds) {
#if RADIX_KERNEL_HAS_PICO_SDK
    if (microseconds == 0) tight_loop_contents();
    else sleep_us(microseconds);
#else
    (void)microseconds;
#endif
}

extern "C" uint32_t rad_hal_core_count(void) {
#if RADIX_KERNEL_HAS_PICO_SDK
    return 2u;
#else
    return 1u;
#endif
}

extern "C" uint32_t rad_hal_current_core(void) {
#if RADIX_KERNEL_HAS_PICO_SDK
    return get_core_num();
#else
    return 0u;
#endif
}

extern "C" rad_status_t rad_hal_start_worker_core(uint32_t core, void (*entry)(uint32_t core)) {
#if RADIX_KERNEL_HAS_PICO_SDK
    if (core != 1u || !entry) return RAD_STATUS_NOT_SUPPORTED;
    if (g_core1_started) return RAD_STATUS_ALREADY_EXISTS;
    g_worker_entry = entry;
    multicore_launch_core1(core1_entry);
    g_core1_started = true;
    return RAD_STATUS_OK;
#else
    (void)core;
    (void)entry;
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}

extern "C" void rad_hal_worker_wait(void) {
#if RADIX_KERNEL_HAS_PICO_SDK
    tight_loop_contents();
#endif
}

extern "C" void rad_hal_worker_wake(void) {
}

extern "C" rad_status_t rad_hal_register_default_devices(void) {
#if RADIX_KERNEL_HAS_PICO_SDK
    stdio_init_all();
    i2c_init(i2c0, 100000u);
    spi_init(spi0, 1000000u);
    gpio_set_function(4u, GPIO_FUNC_I2C);
    gpio_set_function(5u, GPIO_FUNC_I2C);
    gpio_set_function(18u, GPIO_FUNC_SPI);
    gpio_set_function(19u, GPIO_FUNC_SPI);
    gpio_set_function(16u, GPIO_FUNC_SPI);
    gpio_init(17u);
    gpio_set_dir(17u, GPIO_OUT);
    gpio_put(17u, 1u);
    gpio_pull_up(4u);
    gpio_pull_up(5u);
#endif
    rad_device_ops_t serial{};
    serial.read = pico_serial_read;
    serial.write = pico_serial_write;
    serial.ioctl = pico_serial_ioctl;
    register_alias("/dev/console", &serial);
    register_alias("/dev/serial0", &serial);
    register_alias("/dev/uart0", &serial);
    register_alias("/dev/ttyS0", &serial);
    rad_i2c_controller_config_t i2c_config{};
    i2c_config.size = sizeof(i2c_config);
    i2c_config.bus_id = 0;
    i2c_config.name = "rad_i2c_rp235x";
    i2c_config.tree_path = "/soc/i2c@0";
    i2c_config.clock_hz = 100000u;
    i2c_config.sda_gpio = 4u;
    i2c_config.scl_gpio = 5u;
    rad_i2c_controller_ops_t i2c_ops{};
    i2c_ops.transfer = pico_i2c_transfer;
    rad_i2c_controller_register(&i2c_config, &i2c_ops);
    rad_spi_controller_config_t spi_config{};
    spi_config.size = sizeof(spi_config);
    spi_config.bus_id = 0;
    spi_config.name = "rad_spi_rp235x";
    spi_config.tree_path = "/soc/spi@0";
    spi_config.clock_hz = 1000000u;
    spi_config.sck_gpio = 18u;
    spi_config.tx_gpio = 19u;
    spi_config.rx_gpio = 16u;
    spi_config.cs_gpio = 17u;
    rad_spi_controller_ops_t spi_ops{};
    spi_ops.transfer = pico_spi_transfer;
    spi_ops.transfer_dma = pico_spi_transfer_dma;
    rad_spi_controller_register(&spi_config, &spi_ops);
    rad_dma_controller_config_t dma_config{};
    dma_config.size = sizeof(dma_config);
    dma_config.name = "rad_dma_rp235x";
    dma_config.channel_count =
#if RADIX_KERNEL_HAS_PICO_DMA
        12u;
#else
        0u;
#endif
    rad_dma_backend_ops_t dma_ops{};
    dma_ops.request = pico_dma_request;
    dma_ops.release = pico_dma_release;
    dma_ops.submit = pico_dma_submit;
    dma_ops.wait = pico_dma_wait;
    dma_ops.cancel = pico_dma_cancel;
    rad_dma_controller_register(&dma_config, &dma_ops);
    rad_framebuffer_config_t fb_config{};
    fb_config.size = sizeof(fb_config);
    fb_config.name = "/dev/fb0";
    fb_config.info.size = sizeof(fb_config.info);
    fb_config.info.width = 160u;
    fb_config.info.height = 120u;
    fb_config.info.stride_bytes = 160u * 2u;
    fb_config.info.pixel_format = RAD_PIXEL_FORMAT_RGB565;
    fb_config.info.pixels = g_framebuffer;
#if RADIX_KERNEL_ENABLE_RP2350_HSTX_DVI
    fb_config.output_type = RAD_DISPLAY_OUTPUT_RP2350_HSTX;
    fb_config.connector = "rp2350-hstx-dvi";
#else
    fb_config.output_type = RAD_DISPLAY_OUTPUT_MEMORY;
    fb_config.connector = "rp2350-memory";
#endif
    fb_config.mode_count = 1u;
    fb_config.preferred_mode = 0u;
    fb_config.modes[0].width = fb_config.info.width;
    fb_config.modes[0].height = fb_config.info.height;
    fb_config.modes[0].refresh_hz = 60u;
    fb_config.modes[0].stride_bytes = fb_config.info.stride_bytes;
    fb_config.modes[0].pixel_format = fb_config.info.pixel_format;
    fb_config.primary = 1;
    rad_framebuffer_ops_t fb_ops{};
    fb_ops.flush = pico_framebuffer_flush;
    rad_framebuffer_register_ex(&fb_config, &fb_ops);
    rad_terminal_attach_device("/dev/console");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_mount_sd(const rad_sd_config_t *config) {
    if (!config) return RAD_STATUS_INVALID_ARGUMENT;
#if RADIX_KERNEL_ENABLE_SDIO_PIO
    if (config->mode == RAD_SD_MODE_PIO_SDIO || config->mode == RAD_SD_MODE_AUTO) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
#endif
    if (config->mode == RAD_SD_MODE_SPI || config->mode == RAD_SD_MODE_AUTO) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}
