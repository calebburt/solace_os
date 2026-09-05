#include <stdint.h>
#include <stdbool.h>
#include "exceptions/exceptions.h"
#include "lib/stdio.h"
#include "syscall/syscall.h"

// Helper function to read ESR_EL1
static inline uint64_t read_esr_el1(void) {
    uint64_t val;
    asm volatile("mrs %0, esr_el1" : "=r" (val));
    return val;
}

// Helper function to read ELR_EL1
static inline uint64_t read_elr_el1(void) {
    uint64_t val;
    asm volatile("mrs %0, elr_el1" : "=r" (val));
    return val;
}

// Helper function to read FAR_EL1
static inline uint64_t read_far_el1(void) {
    uint64_t val;
    asm volatile("mrs %0, far_el1" : "=r" (val));
    return val;
}

// Simple panic function
void panic(const char *message) {
    kprintf("\nKERNEL PANIC: %s\n", message);
    kprintf("System halted.\n");
    // Disable interrupts here
    asm volatile("msr daifset, #0xf");
    while (1) {
        asm volatile("wfi"); // Wait for interrupt (effectively halt)
    }
}

// Function to print register context
void print_registers(const saved_registers_t *context) {
    kprintf("Saved Registers:\n");
    for (int i = 0; i < 31; i += 2) {
         kprintf("  x%-2d: %016llx   x%-2d: %016llx\n",
                 i, context->regs[i], i + 1, (i + 1 < 31)? context->regs[i + 1] : 0);
    }
     kprintf("  SPSR_EL1: %016llx\n", context->spsr_el1);
     kprintf("  ELR_EL1:  %016llx\n", context->elr_el1);
     kprintf("  SP_EL0:   %016llx\n", context->sp_el0);
}


// --- Exception Handlers ---

// Called by assembly wrapper for synchronous exceptions
void handle_sync_exception(saved_registers_t *context) {
    uint64_t esr = read_esr_el1();
    uint64_t elr = context->elr_el1; // Use saved ELR
    uint64_t far = read_far_el1(); // Fault Address Register

    uint32_t ec = (esr >> 26) & 0x3F; // Extract Exception Class (bits 31:26)
    uint32_t iss = esr & 0x1FFFFFF;   // Extract Instruction Specific Syndrome (bits 24:0)

    const char* ec_str = "Unknown";
    bool far_valid = false;

    switch (ec) {
        case 0b000000: ec_str = "Unknown reason"; break;
        case 0b000001: ec_str = "Trapped WFI or WFE"; break;
        //... other EC values for MCR/MRC, MCRR/MRRC, LDC/STC etc. (AArch32 related)
        case 0b001110: ec_str = "Illegal Execution State"; break;
        case 0b010001: ec_str = "SVC instruction execution in AArch32 state"; break;
        case 0b010101: ec_str = "SVC instruction execution in AArch64 state"; break;
        case 0b011000: ec_str = "Trapped MSR, MRS or System instruction execution in AArch64 state"; break;
        case 0b011001: ec_str = "Access to SVE functionality trapped"; break; // Added in ARMv8.2
        case 0b100000: ec_str = "Instruction Abort from a lower Exception level (AArch32)"; far_valid = true; break;
        case 0b100001: ec_str = "Instruction Abort from a lower Exception level (AArch64)"; far_valid = true; break;
        case 0b100010: ec_str = "PC alignment fault exception"; break;
        case 0b100100: ec_str = "Data Abort from a lower Exception level (AArch32)"; far_valid = true; break;
        case 0b100101: ec_str = "Data Abort from a lower Exception level (AArch64)"; far_valid = true; break;
        case 0b100110: ec_str = "SP alignment fault exception"; break;
        case 0b101000: ec_str = "Trapped floating-point exception (AArch32)"; break;
        case 0b101100: ec_str = "Trapped floating-point exception (AArch64)"; break;
        case 0b110000: ec_str = "SError interrupt"; break;
        case 0b110001: ec_str = "Breakpoint exception from a lower Exception level (AArch32)"; break;
        case 0b110010: ec_str = "Breakpoint exception from a lower Exception level (AArch64)"; break;
        case 0b110100: ec_str = "Step exception from a lower Exception level (AArch32)"; break;
        case 0b110101: ec_str = "Step exception from a lower Exception level (AArch64)"; break;
        case 0b111000: ec_str = "Watchpoint exception from a lower Exception level (AArch32)"; break;
        case 0b111001: ec_str = "Watchpoint exception from a lower Exception level (AArch64)"; break;
        case 0b111100: ec_str = "BRK instruction execution in AArch64 state"; break;
        // Exceptions from current EL
        case 0b100011: ec_str = "Instruction Abort from current EL"; far_valid = true; break;
        case 0b100111: ec_str = "Data Abort from current EL"; far_valid = true; break;
        default: ec_str = "Unhandled Exception Class"; break;
    }

    // Handle specific exceptions or panic
    if (ec == 0b111100) { // BRK instruction
        kprintf("BRK instruction encountered. Continuing execution.\n");
        // Advance ELR_EL1 past the BRK instruction (assuming BRK is 4 bytes)
        context->elr_el1 += 4;
        // Return normally via restore_context -> eret
    } else if (ec == 0b010101) { // SVC instruction
        uint16_t svc_imm = iss & 0xFFFF; // Extract immediate value from ISS
        // Handle the system call based on svc_imm and registers x0-x7 in context
        handle_syscall(svc_imm, context);
    } else {
        kprintf("\n--- Synchronous Exception Taken ---\n");
        kprintf(" ESR_EL1: %016llx (EC: 0x%x, ISS: 0x%x)\n", esr, ec, iss);
        kprintf(" ELR_EL1: %016llx (Return Address)\n", elr);

        kprintf(" Type: %s\n", ec_str);
        if (far_valid) {
            kprintf(" FAR_EL1: %016llx (Faulting Virtual Address)\n", far);
        }
        print_registers(context);
        kprintf("-------------------------------------\n");
        // For most other synchronous exceptions, panic.
        panic("Unhandled synchronous exception");
    }

    // The assembly wrapper restores the frame and executes eret after this
    // function returns.
}

// Called by assembly wrapper for IRQ exceptions
void handle_irq(saved_registers_t *context) {
    kprintf("\n--- IRQ Received ---\n");
    // TODO: Interact with the Generic Interrupt Controller (GIC)
    // 1. Read Interrupt Acknowledge Register (IAR) from CPU interface (GICC_IAR)
    //    to get the interrupt ID and acknowledge the interrupt.
    // 2. Dispatch to the appropriate driver/handler based on the ID.
    // 3. Write to End Of Interrupt Register (EOIR) (GICC_EOIR) to signal completion.
    kprintf(" (No GIC driver implemented yet)\n");
    print_registers(context);
    kprintf("--------------------\n");
    // For now, just return via restore_context -> eret
}

// Placeholder for FIQ
void handle_fiq(saved_registers_t *context) {
    kprintf("\n--- FIQ Received ---\n");
    print_registers(context);
    panic("FIQ handling not implemented");
}

// Placeholder for SError
void handle_serror(saved_registers_t *context) {
    uint64_t esr = read_esr_el1();
    kprintf("\n--- SError Received ---\n");
    kprintf(" ESR_EL1: %016llx\n", esr);
    print_registers(context);
    panic("SError handling not implemented");
}