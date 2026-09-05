#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "exceptions/exceptions.h"

void handle_syscall(uint16_t svc_imm, saved_registers_t *context);

#endif