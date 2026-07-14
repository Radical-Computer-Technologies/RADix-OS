#ifndef RAD_X86_64_GRUB_SLINT_VM_H
#define RAD_X86_64_GRUB_SLINT_VM_H

#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>

typedef struct x86_vm_summary {
    uint64_t usable_bytes;
    uint64_t tracked_pages;
    uint64_t free_pages;
    uint64_t reserved_pages;
    uint64_t kernel_start;
    uint64_t kernel_end;
    int page_allocator_ready;
    int user_address_space_ready;
} x86_vm_summary_t;

typedef struct x86_address_space {
    uint64_t pml4;
    uint64_t owned_pages[16384];
    size_t owned_page_count;
} x86_address_space_t;

extern "C" void x86_vm_init(const rad_boot_info_t *boot, x86_vm_summary_t *summary);
extern "C" uint64_t x86_vm_alloc_page(void);
extern "C" int x86_vm_retain_page(uint64_t physical_address);
extern "C" void x86_vm_free_page(uint64_t physical_address);
extern "C" int x86_vm_create_address_space(x86_address_space_t *space);
extern "C" int x86_vm_map_user_page(x86_address_space_t *space, uint64_t virtual_address, uint64_t physical_address, int writable);
extern "C" int x86_vm_map_shared_page(x86_address_space_t *space, uint64_t virtual_address, uint64_t physical_address, int writable);
extern "C" int x86_vm_map_kernel_mmio(uint64_t physical_address, uint64_t size);
extern "C" int x86_vm_clone_cow(x86_address_space_t *child, x86_address_space_t *parent);
extern "C" int x86_vm_handle_page_fault(uint64_t fault_address, uint64_t error_code);
extern "C" void x86_vm_destroy_address_space(x86_address_space_t *space);
extern "C" void x86_vm_activate_address_space(const x86_address_space_t *space);
extern "C" void x86_vm_activate_kernel_address_space(void);
extern "C" int x86_vm_validate_user_range(uint64_t virtual_address, uint64_t size, int write);
extern "C" int x86_vm_address_space_isolated(const x86_address_space_t *space);
extern "C" int x86_vm_self_test(void);

#endif
