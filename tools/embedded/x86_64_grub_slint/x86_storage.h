#ifndef RAD_X86_64_GRUB_SLINT_STORAGE_H
#define RAD_X86_64_GRUB_SLINT_STORAGE_H

#include <stddef.h>
#include <stdint.h>

typedef struct x86_storage_summary {
    uint32_t pci_devices;
    uint32_t virtio_block_devices;
    uint32_t registered_block_devices;
    uint32_t virtio_net_devices;
    uint32_t registered_net_devices;
    uint8_t virtio_bus;
    uint8_t virtio_slot;
    uint8_t virtio_function;
    uint8_t reserved;
} x86_storage_summary_t;

extern "C" void x86_storage_probe(x86_storage_summary_t *summary);
extern "C" int x86_storage_self_test(void);
extern "C" int x86_network_self_test(void);
extern "C" void x86_virtio_input_poll(void);

#endif
