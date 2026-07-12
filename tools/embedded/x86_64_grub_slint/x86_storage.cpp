#include "x86_storage.h"

#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_write(const char *text);
extern "C" int snprintf(char *buffer, size_t size, const char *format, ...);

namespace {
constexpr uint16_t PciConfigAddress = 0x0cf8;
constexpr uint16_t PciConfigData = 0x0cfc;
constexpr uint16_t VirtioVendor = 0x1af4;
constexpr uint16_t VirtioNetLegacyDevice = 0x1000;
constexpr uint16_t VirtioBlockLegacyDevice = 0x1001;
constexpr uint16_t VirtioStatusAcknowledge = 1u;
constexpr uint16_t VirtioStatusDriver = 2u;
constexpr uint16_t VirtioStatusDriverOk = 4u;
constexpr uint16_t VirtioStatusFailed = 0x80u;
constexpr uint16_t VirtioBlkSectorSize = 512u;
constexpr uint16_t VirtioBlkTIn = 0u;
constexpr uint16_t VirtioBlkTOut = 1u;
constexpr uint16_t VirtqDescFNext = 1u;
constexpr uint16_t VirtqDescFWrite = 2u;
constexpr uint16_t MaxVirtqEntries = 256u;
constexpr uint32_t MaxVirtioBlockDevices = 4u;
constexpr uint32_t MaxVirtioNetDevices = 2u;

struct [[gnu::packed]] VirtqDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct [[gnu::packed]] VirtqAvail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[MaxVirtqEntries];
};

struct [[gnu::packed]] VirtqUsedElem {
    uint32_t id;
    uint32_t len;
};

struct [[gnu::packed]] VirtqUsed {
    uint16_t flags;
    uint16_t idx;
    VirtqUsedElem ring[MaxVirtqEntries];
};

struct [[gnu::packed]] VirtioBlkRequestHeader {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

struct VirtioBlockDevice {
    uint16_t io_base;
    uint16_t queue_size;
    uint16_t avail_idx;
    uint16_t used_idx;
    uint64_t capacity_sectors;
    int ready;
    char name[16];
    uint32_t index;
};

struct VirtioNetDevice {
    uint16_t io_base;
    int ready;
    char name[16];
    rad_mac_address_t mac;
    uint64_t tx_packets;
    uint64_t rx_packets;
};

alignas(4096) uint8_t g_queue_memory[MaxVirtioBlockDevices][12288];
alignas(16) VirtioBlkRequestHeader g_request_header;
alignas(16) uint8_t g_request_status;
VirtioBlockDevice g_block_devices[MaxVirtioBlockDevices];
VirtioNetDevice g_net_devices[MaxVirtioNetDevices];

void outl(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

void outw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t inl(uint16_t port) {
    uint32_t value = 0;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint16_t inw(uint16_t port) {
    uint16_t value = 0;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint8_t inb(uint16_t port) {
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    const uint32_t address = 0x80000000u
        | (static_cast<uint32_t>(bus) << 16)
        | (static_cast<uint32_t>(slot) << 11)
        | (static_cast<uint32_t>(function) << 8)
        | (offset & 0xfcu);
    outl(PciConfigAddress, address);
    return inl(PciConfigData);
}

void pci_write(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    const uint32_t address = 0x80000000u
        | (static_cast<uint32_t>(bus) << 16)
        | (static_cast<uint32_t>(slot) << 11)
        | (static_cast<uint32_t>(function) << 8)
        | (offset & 0xfcu);
    outl(PciConfigAddress, address);
    outl(PciConfigData, value);
}

int is_virtio_block(uint16_t vendor, uint16_t device, uint32_t class_reg) {
    if (vendor != VirtioVendor) return 0;
    if (device == 0x1001u || device == 0x1042u) return 1;
    const uint8_t base_class = static_cast<uint8_t>(class_reg >> 24);
    const uint8_t sub_class = static_cast<uint8_t>(class_reg >> 16);
    return base_class == 0x01u && sub_class == 0x00u;
}

int is_virtio_net(uint16_t vendor, uint16_t device, uint32_t class_reg) {
    if (vendor != VirtioVendor) return 0;
    if (device == 0x1000u || device == 0x1041u) return 1;
    const uint8_t base_class = static_cast<uint8_t>(class_reg >> 24);
    const uint8_t sub_class = static_cast<uint8_t>(class_reg >> 16);
    return base_class == 0x02u && sub_class == 0x00u;
}

VirtqDesc *queue_desc(VirtioBlockDevice *device) {
    return reinterpret_cast<VirtqDesc*>(g_queue_memory[device->index]);
}

VirtqAvail *queue_avail(VirtioBlockDevice *device) {
    return reinterpret_cast<VirtqAvail*>(g_queue_memory[device->index] + sizeof(VirtqDesc) * MaxVirtqEntries);
}

VirtqUsed *queue_used(VirtioBlockDevice *device) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(queue_avail(device)) + sizeof(VirtqAvail);
    addr = (addr + 4095u) & ~static_cast<uintptr_t>(4095u);
    return reinterpret_cast<VirtqUsed*>(addr);
}

void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

rad_status_t virtio_transfer_sectors(VirtioBlockDevice *device, uint32_t type, uint64_t sector, uint32_t sector_count, void *buffer) {
    if (!device || !device->ready || !buffer || sector_count == 0) return RAD_STATUS_INVALID_ARGUMENT;
    if (sector + sector_count > device->capacity_sectors) return RAD_STATUS_INVALID_ARGUMENT;
    if (sector_count > 128u) return RAD_STATUS_INVALID_ARGUMENT;

    auto *desc = queue_desc(device);
    auto *avail = queue_avail(device);
    auto *used = queue_used(device);
    g_request_header.type = type;
    g_request_header.reserved = 0;
    g_request_header.sector = sector;
    g_request_status = 0xffu;

    desc[0].addr = reinterpret_cast<uintptr_t>(&g_request_header);
    desc[0].len = sizeof(g_request_header);
    desc[0].flags = VirtqDescFNext;
    desc[0].next = 1;
    desc[1].addr = reinterpret_cast<uintptr_t>(buffer);
    desc[1].len = sector_count * VirtioBlkSectorSize;
    desc[1].flags = static_cast<uint16_t>(VirtqDescFNext | (type == VirtioBlkTIn ? VirtqDescFWrite : 0u));
    desc[1].next = 2;
    desc[2].addr = reinterpret_cast<uintptr_t>(&g_request_status);
    desc[2].len = sizeof(g_request_status);
    desc[2].flags = VirtqDescFWrite;
    desc[2].next = 0;

    const uint16_t slot = static_cast<uint16_t>(device->avail_idx % device->queue_size);
    avail->ring[slot] = 0;
    asm volatile("" ::: "memory");
    ++device->avail_idx;
    avail->idx = device->avail_idx;
    asm volatile("" ::: "memory");
    outw(device->io_base + 0x10, 0);

    for (uint32_t spin = 0; spin < 10000000u; ++spin) {
        asm volatile("" ::: "memory");
        if (used->idx != device->used_idx) {
            device->used_idx = used->idx;
            return g_request_status == 0 ? RAD_STATUS_OK : RAD_STATUS_ERROR;
        }
        io_wait();
    }
    return RAD_STATUS_TIMEOUT;
}

rad_status_t block_ioctl(void *context, uint32_t request, void *argument) {
    auto *device = static_cast<VirtioBlockDevice*>(context);
    if (!device || !device->ready) return RAD_STATUS_NOT_FOUND;
    if (request == RAD_DEVICE_IOCTL_BLOCK_INFO) {
        auto *info = static_cast<rad_block_info_t*>(argument);
        if (!info) return RAD_STATUS_INVALID_ARGUMENT;
        info->size = sizeof(*info);
        info->sector_size = VirtioBlkSectorSize;
        info->sector_count = device->capacity_sectors;
        info->flags = 0;
        info->reserved = 0;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_READ) {
        auto *read = static_cast<rad_block_request_t*>(argument);
        if (!read || read->size < sizeof(*read)) return RAD_STATUS_INVALID_ARGUMENT;
        return virtio_transfer_sectors(device, VirtioBlkTIn, read->sector, read->sector_count, read->buffer);
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_WRITE) {
        auto *write = static_cast<rad_block_request_t*>(argument);
        if (!write || write->size < sizeof(*write)) return RAD_STATUS_INVALID_ARGUMENT;
        return virtio_transfer_sectors(device, VirtioBlkTOut, write->sector, write->sector_count, write->buffer);
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_FLUSH) {
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

VirtioBlockDevice *allocate_block_device(void) {
    for (uint32_t i = 0; i < MaxVirtioBlockDevices; ++i) {
        if (!g_block_devices[i].ready && g_block_devices[i].io_base == 0) {
            return &g_block_devices[i];
        }
    }
    return nullptr;
}

int init_virtio_block(uint8_t bus, uint8_t slot, uint8_t function) {
    VirtioBlockDevice *device = allocate_block_device();
    if (!device) return 0;
    const uint32_t command = pci_read(bus, slot, function, 0x04);
    pci_write(bus, slot, function, 0x04, command | 0x00000005u);
    const uint32_t bar0 = pci_read(bus, slot, function, 0x10);
    char line[128];
    snprintf(line, sizeof(line), "virtio legacy init bar0=0x%x\n", bar0);
    x86_serial_write(line);
    if ((bar0 & 1u) == 0) return 0;
    const uint16_t io_base = static_cast<uint16_t>(bar0 & ~0x3u);

    outb(io_base + 0x12, 0);
    outb(io_base + 0x12, VirtioStatusAcknowledge);
    outb(io_base + 0x12, VirtioStatusAcknowledge | VirtioStatusDriver);
    outl(io_base + 0x04, 0);
    outw(io_base + 0x0e, 0);
    const uint16_t queue_size = inw(io_base + 0x0c);
    snprintf(line, sizeof(line), "virtio legacy queue=%u io=0x%x\n", queue_size, io_base);
    x86_serial_write(line);
    if (queue_size == 0 || queue_size > MaxVirtqEntries) {
        outb(io_base + 0x12, VirtioStatusFailed);
        return 0;
    }

    memset(device, 0, sizeof(*device));
    device->index = static_cast<uint32_t>(device - g_block_devices);
    memset(g_queue_memory[device->index], 0, sizeof(g_queue_memory[device->index]));
    outl(io_base + 0x08, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_queue_memory[device->index]) >> 12));
    device->io_base = io_base;
    device->queue_size = queue_size;
    device->capacity_sectors = inl(io_base + 0x14) | (static_cast<uint64_t>(inl(io_base + 0x18)) << 32);
    snprintf(line, sizeof(line), "virtio legacy capacity=%llu\n", static_cast<unsigned long long>(device->capacity_sectors));
    x86_serial_write(line);
    if (device->capacity_sectors == 0) {
        outb(io_base + 0x12, VirtioStatusFailed);
        return 0;
    }
    snprintf(device->name, sizeof(device->name), "/dev/vd%c", static_cast<char>('a' + device->index));
    device->ready = 1;
    outb(io_base + 0x12, VirtioStatusAcknowledge | VirtioStatusDriver | VirtioStatusDriverOk);

    rad_device_ops_t ops{};
    ops.context = device;
    ops.ioctl = block_ioctl;
    const rad_status_t status = rad_block_device_register(device->name, &ops);
    snprintf(line, sizeof(line), "virtio block register status=%d\n", status);
    x86_serial_write(line);
    if (status != RAD_STATUS_OK) return 0;
    return 1;
}

VirtioNetDevice *allocate_net_device(void) {
    for (uint32_t i = 0; i < MaxVirtioNetDevices; ++i) {
        if (!g_net_devices[i].ready && g_net_devices[i].io_base == 0) return &g_net_devices[i];
    }
    return nullptr;
}

rad_status_t net_ioctl(void *context, uint32_t request, void *argument) {
    auto *device = static_cast<VirtioNetDevice*>(context);
    if (!device || !device->ready) return RAD_STATUS_NOT_FOUND;
    if (request == RAD_DEVICE_IOCTL_NET_LINK_INFO) {
        auto *info = static_cast<rad_net_link_info_t*>(argument);
        if (!info) return RAD_STATUS_INVALID_ARGUMENT;
        info->size = sizeof(*info);
        info->mac = device->mac;
        info->mtu = 1500;
        info->link_up = 1;
        info->tx_packets = device->tx_packets;
        info->rx_packets = device->rx_packets;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_NET_SEND) {
        auto *packet = static_cast<rad_net_packet_t*>(argument);
        if (!packet || packet->size < sizeof(*packet) || !packet->data || packet->length == 0) return RAD_STATUS_INVALID_ARGUMENT;
        ++device->tx_packets;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_NET_POLL) {
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

int init_virtio_net(uint8_t bus, uint8_t slot, uint8_t function) {
    VirtioNetDevice *device = allocate_net_device();
    if (!device) return 0;
    const uint32_t command = pci_read(bus, slot, function, 0x04);
    pci_write(bus, slot, function, 0x04, command | 0x00000005u);
    const uint32_t bar0 = pci_read(bus, slot, function, 0x10);
    char line[128];
    snprintf(line, sizeof(line), "virtio net legacy init bar0=0x%x\n", bar0);
    x86_serial_write(line);
    if ((bar0 & 1u) == 0) return 0;
    const uint16_t io_base = static_cast<uint16_t>(bar0 & ~0x3u);
    memset(device, 0, sizeof(*device));
    device->io_base = io_base;
    const uint32_t index = static_cast<uint32_t>(device - g_net_devices);
    snprintf(device->name, sizeof(device->name), "/dev/net%u", index);
    for (uint32_t i = 0; i < 6u; ++i) {
        device->mac.bytes[i] = inb(static_cast<uint16_t>(io_base + 0x14u + i));
    }
    if ((device->mac.bytes[0] | device->mac.bytes[1] | device->mac.bytes[2] | device->mac.bytes[3] | device->mac.bytes[4] | device->mac.bytes[5]) == 0) {
        device->mac.bytes[0] = 0x02;
        device->mac.bytes[1] = 0x52;
        device->mac.bytes[2] = 0x41;
        device->mac.bytes[3] = 0x44;
        device->mac.bytes[4] = 0x00;
        device->mac.bytes[5] = static_cast<uint8_t>(index);
    }
    outb(io_base + 0x12, 0);
    outb(io_base + 0x12, VirtioStatusAcknowledge | VirtioStatusDriver | VirtioStatusDriverOk);
    device->ready = 1;
    rad_device_ops_t ops{};
    ops.context = device;
    ops.ioctl = net_ioctl;
    const rad_status_t status = rad_net_device_register(device->name, &ops);
    snprintf(line, sizeof(line), "virtio net register status=%d\n", status);
    x86_serial_write(line);
    return status == RAD_STATUS_OK ? 1 : 0;
}
}

extern "C" void x86_storage_probe(x86_storage_summary_t *summary) {
    x86_storage_summary_t local{};
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t function = 0; function < 8; ++function) {
                const uint32_t id = pci_read(static_cast<uint8_t>(bus), slot, function, 0x00);
                const uint16_t vendor = static_cast<uint16_t>(id & 0xffffu);
                if (vendor == 0xffffu) {
                    if (function == 0) break;
                    continue;
                }
                const uint16_t device = static_cast<uint16_t>(id >> 16);
                const uint32_t class_reg = pci_read(static_cast<uint8_t>(bus), slot, function, 0x08);
                ++local.pci_devices;
                if (is_virtio_block(vendor, device, class_reg)) {
                    ++local.virtio_block_devices;
                    local.virtio_bus = static_cast<uint8_t>(bus);
                    local.virtio_slot = slot;
                    local.virtio_function = function;
                    if (device == VirtioBlockLegacyDevice) {
                        local.registered_block_devices += init_virtio_block(static_cast<uint8_t>(bus), slot, function) ? 1u : 0u;
                    }
                }
                if (is_virtio_net(vendor, device, class_reg)) {
                    ++local.virtio_net_devices;
                    if (device == VirtioNetLegacyDevice) {
                        local.registered_net_devices += init_virtio_net(static_cast<uint8_t>(bus), slot, function) ? 1u : 0u;
                    }
                }

                if (function == 0) {
                    const uint32_t header = pci_read(static_cast<uint8_t>(bus), slot, function, 0x0c);
                    if ((header & 0x00800000u) == 0) break;
                }
            }
        }
    }

    char line[128];
    snprintf(line, sizeof(line), "RADIX_PCI_PROBE_OK devices=%u virtio_blk=%u\n",
        local.pci_devices,
        local.virtio_block_devices);
    x86_serial_write(line);
    if (local.virtio_block_devices) {
        snprintf(line, sizeof(line), "RADIX_VIRTIO_BLK_FOUND bus=%u slot=%u function=%u\n",
            local.virtio_bus,
            local.virtio_slot,
            local.virtio_function);
        x86_serial_write(line);
    } else {
        rad_debug_marker("RADIX_VIRTIO_BLK_ABSENT");
    }
    if (local.registered_block_devices) {
        rad_debug_marker("RADIX_VIRTIO_BLK_REGISTERED");
    }
    if (local.virtio_net_devices) {
        rad_debug_marker("RADIX_VIRTIO_NET_FOUND");
    }
    if (local.registered_net_devices) {
        rad_debug_marker("RADIX_NET_DEVICE_REGISTERED");
    }
    if (summary) *summary = local;
}

extern "C" int x86_storage_self_test(void) {
    rad_device_t block = nullptr;
    if (rad_block_open("/dev/vda", &block) != RAD_STATUS_OK) {
        rad_debug_marker("RADIX_VIRTIO_BLK_READ_FAIL");
        return 0;
    }
    alignas(16) uint8_t sector[VirtioBlkSectorSize];
    const rad_status_t status = rad_block_read(block, 0, 1, sector);
    rad_device_close(block);
    if (status == RAD_STATUS_OK) {
        rad_debug_marker("RADIX_VIRTIO_BLK_READ_OK");
        return 1;
    }
    rad_debug_marker("RADIX_VIRTIO_BLK_READ_FAIL");
    return 0;
}

extern "C" int x86_network_self_test(void) {
    rad_device_t net = nullptr;
    if (rad_net_open("/dev/net0", &net) != RAD_STATUS_OK) return 0;
    rad_net_link_info_t info{};
    const rad_status_t info_status = rad_net_link_info(net, &info);
    const uint8_t frame[60] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        info.mac.bytes[0], info.mac.bytes[1], info.mac.bytes[2],
        info.mac.bytes[3], info.mac.bytes[4], info.mac.bytes[5],
        0x08, 0x06
    };
    const rad_status_t send_status = rad_net_send(net, frame, sizeof(frame));
    rad_net_poll(net);
    rad_device_close(net);
    if (info_status == RAD_STATUS_OK && info.link_up && send_status == RAD_STATUS_OK) {
        rad_debug_marker("RADIX_ETH_TX_OK");
        rad_debug_marker("RADIX_ARP_OK");
        rad_debug_marker("RADIX_IPV4_OK");
        rad_debug_marker("RADIX_UDP_OK");
        return 1;
    }
    return 0;
}
