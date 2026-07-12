.section .text.boot, "ax"
.global _start
.type _start, %function

_start:
    msr daifset, #0xf
    ldr x30, =__stack_top
    mov sp, x30
    bl radix_pi_entry

1:
    wfe
    b 1b

.section .bss.stack, "aw", %nobits
.align 12
.global __stack_bottom
__stack_bottom:
    .skip 65536
.global __stack_top
__stack_top:
