#include "syscall/syscall.h"
#include "lib/stdio.h"

void handle_syscall(uint16_t svc_imm, saved_registers_t *context) {
    // For demonstration, we'll just print the syscall number and the first few arguments.
    kprintf("Handling syscall: 0x%x\n", svc_imm);
    kprintf("Arguments (x0-x3): %016llx %016llx %016llx %016llx\n",
            context->regs[0], context->regs[1], context->regs[2], context->regs[3]);

    // Here you would implement the actual syscall handling logic based on svc_imm.
    // For now, we will just return to the caller by advancing ELR_EL1.
    context->elr_el1 += 4; // Advance ELR_EL1 to skip the SVC instruction and return to the next instruction.
}
