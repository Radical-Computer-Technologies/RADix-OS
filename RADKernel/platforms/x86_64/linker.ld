ENTRY(_start)

SECTIONS
{
    . = 1M;
    __kernel_start = .;
    .multiboot2 : { KEEP(*(.multiboot2)) }
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .init_array : {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    }
    .bss : {
        *(COMMON)
        *(.bss*)
    }
    /DISCARD/ : { *(.eh_frame*) *(.comment*) }
    __kernel_end = .;
}
