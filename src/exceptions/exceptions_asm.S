.section ".text.exceptions"
.align 11 // Align to 2KB (2^11)

.globl _exception_vector_table
_exception_vector_table:
    // Handlers for exceptions from Current EL using SP0
   .align 7 // Align each entry to 128 bytes (2^7)
    b sync_handler_sp0      // Synchronous EL1t
   .align 7
    b irq_handler_sp0       // IRQ EL1t
   .align 7
    b fiq_handler_sp0       // FIQ EL1t
   .align 7
    b serror_handler_sp0    // SError EL1t

    // Handlers for exceptions from Current EL using SPx (SP_EL1)
   .align 7
    b sync_handler_spx      // Synchronous EL1h
   .align 7
    b irq_handler_spx       // IRQ EL1h
   .align 7
    b fiq_handler_spx       // FIQ EL1h
   .align 7
    b serror_handler_spx    // SError EL1h

    // Handlers for exceptions from Lower EL (EL0) using AArch64
   .align 7
    b sync_handler_el0_64   // Synchronous EL0 (64-bit)
   .align 7
    b irq_handler_el0_64    // IRQ EL0 (64-bit)
   .align 7
    b fiq_handler_el0_64    // FIQ EL0 (64-bit)
   .align 7
    b serror_handler_el0_64 // SError EL0 (64-bit)

    // Handlers for exceptions from Lower EL (EL0) using AArch32 (Placeholder)
   .align 7
    b sync_handler_el0_32   // Synchronous EL0 (32-bit)
   .align 7
    b irq_handler_el0_32    // IRQ EL0 (32-bit)
   .align 7
    b fiq_handler_el0_32    // FIQ EL0 (32-bit)
   .align 7
    b serror_handler_el0_32 // SError EL0 (32-bit)


// Common entry point macro for saving context
// Assumes exception taken to EL1 using SP_EL1 (current SP)
.macro save_context
    // Allocate space on the stack for GPRs (x0-x30), SPSR_EL1, ELR_EL1, SP_EL0
    // 31 GPRs + 3 system regs = 34 registers * 8 bytes/reg = 272 bytes
    // Exception entry preserves the required 16-byte stack alignment.
    // Save x0 and x1 before using x0 as the context pointer.
    sub sp, sp, #288       // Allocate space (272 + padding for alignment)

    // Store GPRs x0-x30 (31 registers)
    stp x0, x1, [sp, #16 * 0]
    stp x2, x3, [sp, #16 * 1]
    stp x4, x5, [sp, #16 * 2]
    stp x6, x7, [sp, #16 * 3]
    stp x8, x9, [sp, #16 * 4]
    stp x10, x11, [sp, #16 * 5]
    stp x12, x13, [sp, #16 * 6]
    stp x14, x15, [sp, #16 * 7]
    stp x16, x17, [sp, #16 * 8]
    stp x18, x19, [sp, #16 * 9]
    stp x20, x21, [sp, #16 * 10]
    stp x22, x23, [sp, #16 * 11]
    stp x24, x25, [sp, #16 * 12]
    stp x26, x27, [sp, #16 * 13]
    stp x28, x29, [sp, #16 * 14]
    str x30, [sp, #16 * 15] // Store LR (x30)

    // Store relevant system registers
    mrs x0, spsr_el1
    mrs x1, elr_el1
    mrs x2, sp_el0
    stp x0, x1, [sp, #16 * 15 + 8]  // Store SPSR_EL1, ELR_EL1
    str x2, [sp, #16 * 16 + 8]      // Store SP_EL0

    // Pass pointer to saved registers (current SP) to C handler in x0
    mov x0, sp
.endm

// Common exit point macro for restoring context
.macro restore_context
    // x0 might contain return value from C handler, but we don't use it here.
    // Restore system registers first
    ldp x0, x1, [sp, #16 * 15 + 8]  // Load SPSR_EL1, ELR_EL1
    ldr x2, [sp, #16 * 16 + 8]      // Load SP_EL0
    msr spsr_el1, x0
    msr elr_el1, x1
    msr sp_el0, x2

    // Restore GPRs x0-x30
    ldp x0, x1, [sp, #16 * 0]
    ldp x2, x3, [sp, #16 * 1]
    ldp x4, x5, [sp, #16 * 2]
    ldp x6, x7, [sp, #16 * 3]
    ldp x8, x9, [sp, #16 * 4]
    ldp x10, x11, [sp, #16 * 5]
    ldp x12, x13, [sp, #16 * 6]
    ldp x14, x15, [sp, #16 * 7]
    ldp x16, x17, [sp, #16 * 8]
    ldp x18, x19, [sp, #16 * 9]
    ldp x20, x21, [sp, #16 * 10]
    ldp x22, x23, [sp, #16 * 11]
    ldp x24, x25, [sp, #16 * 12]
    ldp x26, x27, [sp, #16 * 13]
    ldp x28, x29, [sp, #16 * 14]
    ldr x30, [sp, #16 * 15] // Restore LR (x30)

    // Deallocate stack space used for saving context
    add sp, sp, #288        // Must match allocation size

    eret // Return from exception
.endm

// --- Specific Handlers ---
// These handlers assume the exception was taken to EL1 using SP_EL1

sync_handler_common:
    save_context
    bl handle_sync_exception // Call C handler
    restore_context

irq_handler_common:
    save_context
    bl handle_irq           // Call C handler
    restore_context

fiq_handler_common:
    save_context
    bl handle_fiq           // Call C handler
    restore_context

serror_handler_common:
    save_context
    bl handle_serror        // Call C handler
    restore_context

// --- Vector Table Entry Points ---
// Route different vector entries to appropriate common handlers
// For simplicity, this example routes most EL1/EL0_64 entries to common handlers.
// A real OS might need more differentiation based on SP0/SPx.
sync_handler_sp0:
sync_handler_spx:
sync_handler_el0_64:
    b sync_handler_common

irq_handler_sp0:
irq_handler_spx:
irq_handler_el0_64:
    b irq_handler_common

fiq_handler_sp0:
fiq_handler_spx:
fiq_handler_el0_64:
    b fiq_handler_common

serror_handler_sp0:
serror_handler_spx:
serror_handler_el0_64:
    b serror_handler_common

// Placeholder handlers for AArch32 (just loop indefinitely)
sync_handler_el0_32:
irq_handler_el0_32:
fiq_handler_el0_32:
serror_handler_el0_32:
    wfi
    b .

// --- External C function declarations ---
.globl handle_sync_exception
.globl handle_irq
.globl handle_fiq
.globl handle_serror