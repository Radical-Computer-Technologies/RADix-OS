#ifndef RADIX_X86_CPU_H
#define RADIX_X86_CPU_H

#include <stdint.h>

#include <radixkernel/radixkernel.h>

extern "C" void x86_cpu_init(uint64_t kernel_stack_top);
extern "C" void x86_cpu_init_ap(uint32_t core);
extern "C" void x86_cpu_set_kernel_stack_top(uint64_t kernel_stack_top);
extern "C" int x86_cpu_self_test(void);
extern "C" int x86_context_self_test(void);
extern "C" void x86_irq_enable_interrupts(void);
extern "C" void x86_cpu_set_topology(uint32_t detected_cores, uint32_t present_mask, uint64_t lapic_address);
extern "C" uint32_t x86_cpu_detected_core_count(void);
extern "C" uint32_t x86_cpu_present_core_mask(void);
extern "C" uint64_t x86_cpu_lapic_address(void);
extern "C" uint32_t x86_cpu_current_core(void);
extern "C" uint32_t x86_cpu_started_core_mask(void);
extern "C" rad_status_t x86_cpu_start_worker_core(uint32_t core, void (*entry)(uint32_t core));
extern "C" void x86_lapic_timer_start_current_core(void);
extern "C" void x86_lapic_timer_dispatch(uint64_t interrupted_cs);

#endif
