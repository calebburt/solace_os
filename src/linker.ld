/* Linker script for Arm-OS */

/* QEMU virt machine memory layout */
ENTRY(_start)

SECTIONS
{
    /* QEMU -kernel loads the raw image at 0x40080000. */
    . = 0x40080000;
    _kernel_start = .;

    .text : ALIGN(8)
    {
        *(.text.boot)  /* Boot code first */
        *(.text)       /* All other code */
        *(.text.*)     /* Including subsections */
    }
    _text_end = .;

    .rodata : ALIGN(8)
    {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    }

    .data : ALIGN(8)
    {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }

    /* Add symbols for load addresses (where sections are actually loaded by QEMU) */
    _rodata_load = LOADADDR(.rodata);
    _data_load = LOADADDR(.data);

    .bss : ALIGN(8)
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    }

    .eh_frame : 
    {
        *(.eh_frame)
    }

    . = ALIGN(8);
    
    /* Reserve space for the stack */
    . = ALIGN(16);
    _stack_bottom = .;
    . += 0x10000; /* 64 KB for stack */
    . = ALIGN(16);
    _stack_top = .;

    /* Reserve space for PMM bitmap */
    . = ALIGN(4096);
    _pmm_bitmap_start = .;
    . += 0x20000; /* 128 KB for PMM bitmap (can manage 4GB of RAM) */
    _pmm_bitmap_end = .;

    _kernel_end = .;
}