/* AArch64 boot code for QEMU virt machine */
.section ".text.boot"

.global _start
_start:
    // Check processor ID is 0 (primary core)
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbz     x0, primary_core
    // Secondary cores loop forever
1:  wfe
    b       1b

primary_core:
    // Set stack pointer to _stack_top
    ldr     x0, =_stack_top
    mov     sp, x0
    
    // Copy .rodata section from load address to execution address
    ldr     x0, =_rodata_start     // Destination address
    ldr     x1, =_rodata_load      // Source address
    ldr     x2, =_rodata_end
    sub     x2, x2, x0             // Size to copy
    bl      copy_data_section
    
    // Copy .data section from load address to execution address
    ldr     x0, =_data_start       // Destination address
    ldr     x1, =_data_load        // Source address
    ldr     x2, =_data_end
    sub     x2, x2, x0             // Size to copy
    bl      copy_data_section
    
    // Clear BSS
    ldr     x0, =_bss_start
    ldr     x1, =_bss_end
    sub     x1, x1, x0
    cbz     x1, skip_bss_clear
clear_bss_loop:
    str     xzr, [x0], #8
    sub     x1, x1, #8
    cbnz    x1, clear_bss_loop
skip_bss_clear:

    // Set up exception vector table
    ldr     x0, =_exception_vector_table
    msr     vbar_el1, x0
    isb

    // Set up EL1 (kernel mode)
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #1
    beq     already_in_el1

    // If we're in EL2, configure EL1 and drop to it
    /* Disable EL1 timer traps */
    mov     x0, #0x3       // CNTHCTL_EL2.EL1PCTEN | CNTHCTL_EL2.EL1PCEN
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr
    
    /* Set EL1 execution state to AArch64 */
    mov     x0, #(1 << 31)      // AArch64
    orr     x0, x0, #(1 << 1)   // SWIO hardwired
    msr     hcr_el2, x0
    
    /* Configure EL1 */
    // Set up SPSR_EL2 for the transition to EL1
    mov     x0, #0x3c5         // DAIF + EL1h (SPSel = 1)
    msr     spsr_el2, x0
    
    /* Set EL1 entry point and switch */
    ldr     x0, =already_in_el1
    msr     elr_el2, x0
    eret

already_in_el1:
    // Enable floating point
    mov     x0, #0x00300000     // FPEN bits
    msr     cpacr_el1, x0
    isb

    // Jump to C code, passing boot info pointer
    mov     x0, #0              // For now, pass null pointer as boot info
    bl      kernel_main         // Call our C entry point

    // If kernel_main returns, halt the CPU
1:  wfe
    b       1b
    
//------------------------------------------------------------------
// Helper function to copy data sections
// x0 = destination address
// x1 = source address
// x2 = size in bytes (must be multiple of 8)
//------------------------------------------------------------------
copy_data_section:
    // Return immediately if source and destination are the same
    cmp     x0, x1
    beq     copy_done
    
    // Return if size is zero
    cbz     x2, copy_done
    
    // Add debugging output
    // Preserve registers used by the output routine
    stp     x0, x1, [sp, #-16]!
    stp     x2, x30, [sp, #-16]!
    
    // Call C debug function to print details about the copy
    mov     x3, x2            // Size
    bl      boot_debug_copy   // Call C function to print debug info
    
    // Restore registers
    ldp     x2, x30, [sp], #16
    ldp     x0, x1, [sp], #16
    
    // Perform the copy, byte by byte for safety
copy_byte_loop:
    ldrb    w4, [x1], #1     // Load a byte
    strb    w4, [x0], #1     // Store a byte
    sub     x2, x2, #1       // Decrement size
    cbnz    x2, copy_byte_loop
    
copy_done:
    ret