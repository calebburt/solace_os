#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel.h"
#include "shell/shell.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/stdlib_stubs.h"
#include "memory/frame_alloc.h"
#include "memory/kheap.h"

#define MAX_CMD_LEN 128
#define MAX_ARGS 10

// --- Address Validation ---
// Needs access to PMM's knowledge of valid RAM regions.
// This is a simplified check against the highest known usable address.
bool is_address_valid(uint64_t addr, size_t len) {
    uint64_t highest_ram = pmm_get_highest_usable_address();
    // Basic check: ensure start and end are within the known usable range
    // and don't wrap around. Assumes PMM_RAM_BASE is the lowest valid RAM addr.
    if (addr < PMM_RAM_BASE || addr >= highest_ram) {
        return false;
    }
    if (len > 0 && (addr + len - 1) >= highest_ram) {
         // Check for overflow before checking end boundary
         if (addr + len < addr) return false; // Overflow occurred
         return false; // End address is out of bounds
    }
    return true;
}


// --- Shell Commands ---

void cmd_memdump(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: memdump <address> [length]\n");
        return;
    }

    char *endptr;
    uint64_t addr = simple_strtoull(argv[1], &endptr, 0);
    if (*endptr != '\0') {
        kprintf("Error: Invalid address format '%s'\n", argv[1]);
        return;
    }

    size_t length = 256; // Default length
    if (argc > 2) {
        length = (size_t)simple_strtoull(argv[2], &endptr, 0);
        if (*endptr != '\0') {
            kprintf("Error: Invalid length format '%s'\n", argv[2]);
            return;
        }
    }

    if (length == 0) return;

    // Validate the entire range
    if (!is_address_valid(addr, length)) {
        kprintf("Error: Address range 0x%llx - 0x%llx is not within valid RAM.\n",
                addr, addr + length -1);
        return;
    }

    kprintf("Memory dump from 0x%llx (length %zu):\n", addr, length);

    volatile uint8_t *ptr = (volatile uint8_t *)addr;
    for (size_t i = 0; i < length; i += 16) {
        kprintf("%016llx: ", addr + i);
        // Print hex bytes
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < length) {
                kprintf("%02x ", ptr[i + j]);
            } else {
                kprintf("   ");
            }
            if (j == 7) kprintf(" "); // Add extra space in the middle
        }
        kprintf(" |");
        // Print ASCII chars
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < length) {
                char c = ptr[i + j];
                kprintf("%c", (c >= 32 && c <= 126)? c : '.');
            } else {
                kprintf(" ");
            }
        }
        kprintf("|\n");
    }
}

void cmd_peek(int argc, char **argv) {
     if (argc < 2) {
        kprintf("Usage: peek <address> [size: b/h/w/d (default: d)]\n");
        return;
    }

    char *endptr;
    uint64_t addr = simple_strtoull(argv[1], &endptr, 0);
     if (*endptr != '\0') {
        kprintf("Error: Invalid address format '%s'\n", argv[1]);
        return;
    }

    char size_char = 'd'; // Default to double word (64-bit)
    size_t size_bytes = 8;
    if (argc > 2) {
        size_char = argv[2][0];
        if (strlen(argv[2]) != 1) {
             kprintf("Error: Invalid size format '%s'\n", argv[2]);
             return;
        }
    }

    switch (size_char) {
        case 'b': size_bytes = 1; break; // Byte
        case 'h': size_bytes = 2; break; // Half-word (16-bit)
        case 'w': size_bytes = 4; break; // Word (32-bit)
        case 'd': size_bytes = 8; break; // Double-word (64-bit)
        default:
            kprintf("Error: Invalid size '%c'. Use b, h, w, or d.\n", size_char);
            return;
    }

     // Validate address for the specified size
    if (!is_address_valid(addr, size_bytes)) {
        kprintf("Error: Address 0x%llx is not within valid RAM for size %zu.\n", addr, size_bytes);
        return;
    }

    // Ensure address alignment for larger types
    if ((size_bytes > 1) && (addr % size_bytes != 0)) {
        kprintf("Warning: Address 0x%llx is not aligned for size %zu.\n", addr, size_bytes);
        // Proceeding might cause alignment fault depending on CPU config / EL
    }

    uint64_t value = 0;
    volatile void *ptr = (volatile void *)addr;

    kprintf("Peek at 0x%llx (size %zu): ", addr, size_bytes);

    switch (size_bytes) {
        case 1: value = *(volatile uint8_t*)ptr; kprintf("0x%02llx\n", value); break;
        case 2: value = *(volatile uint16_t*)ptr; kprintf("0x%04llx\n", value); break;
        case 4: value = *(volatile uint32_t*)ptr; kprintf("0x%08llx\n", value); break;
        case 8: value = *(volatile uint64_t*)ptr; kprintf("0x%016llx\n", value); break;
    }
}

void cmd_poke(int argc, char **argv) {
    if (argc < 3) {
        kprintf("Usage: poke <address> <value> [size: b/h/w/d (default: d)]\n");
        return;
    }

    char *endptr;
    uint64_t addr = simple_strtoull(argv[1], &endptr, 0);
    if (*endptr != '\0') {
        kprintf("Error: Invalid address format '%s'\n", argv[1]);
        return;
    }

    uint64_t value = simple_strtoull(argv[2], &endptr, 0);
     if (*endptr != '\0') {
        kprintf("Error: Invalid value format '%s'\n", argv[2]);
        return;
    }

    char size_char = 'd'; // Default to double word (64-bit)
    size_t size_bytes = 8;
    if (argc > 3) {
        size_char = argv[3][0];
        if (strlen(argv[3]) != 1) {
             kprintf("Error: Invalid size format '%s'\n", argv[3]);
             return;
        }
    }

     switch (size_char) {
        case 'b': size_bytes = 1; break; // Byte
        case 'h': size_bytes = 2; break; // Half-word (16-bit)
        case 'w': size_bytes = 4; break; // Word (32-bit)
        case 'd': size_bytes = 8; break; // Double-word (64-bit)
        default:
            kprintf("Error: Invalid size '%c'. Use b, h, w, or d.\n", size_char);
            return;
    }

    // Validate address for the specified size
    if (!is_address_valid(addr, size_bytes)) {
        kprintf("Error: Address 0x%llx is not within valid RAM for size %zu.\n", addr, size_bytes);
        return;
    }

    // Ensure address alignment for larger types
    if ((size_bytes > 1) && (addr % size_bytes != 0)) {
        kprintf("Warning: Address 0x%llx is not aligned for size %zu.\n", addr, size_bytes);
        // Proceeding might cause alignment fault depending on CPU config / EL
    }

    volatile void *ptr = (volatile void *)addr;

    kprintf("Poke at 0x%llx (size %zu) with value 0x%llx\n", addr, size_bytes, value);

    switch (size_bytes) {
        case 1: *(volatile uint8_t*)ptr = (uint8_t)value; break;
        case 2: *(volatile uint16_t*)ptr = (uint16_t)value; break;
        case 4: *(volatile uint32_t*)ptr = (uint32_t)value; break;
        case 8: *(volatile uint64_t*)ptr = (uint64_t)value; break;
    }
}

void cmd_alloc(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: alloc <size>\n");
        return;
    }

    char *endptr;
    size_t size = (size_t)simple_strtoull(argv[1], &endptr, 0);
    if (*endptr != '\0' || size == 0) {
        kprintf("Error: Invalid size '%s'\n", argv[1]);
        return;
    }

    void *ptr = kmalloc(size);
    if (ptr) {
        kprintf("Allocated %zu bytes at 0x%p\n", size, ptr);
    } else {
        kprintf("Allocation failed!\n");
    }
}

void cmd_free(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: free <address>\n");
        return;
    }

    char *endptr;
    uint64_t addr = simple_strtoull(argv[1], &endptr, 0);
    if (*endptr != '\0') {
        kprintf("Error: Invalid address format '%s'\n", argv[1]);
        return;
    }

    void *ptr = (void *)addr;
    kprintf("Freeing memory at 0x%p\n", ptr);
    kfree(ptr);
}

void cmd_help(int argc, char **argv) {
    kprintf("Available commands:\n");
    kprintf("  help          - Display this help message\n");
    kprintf("  memdump <addr> [len] - Dump memory contents (default len=256)\n");
    kprintf("  peek <addr> [sz] - Read value from memory (sz=b/h/w/d, default=d)\n");
    kprintf("  poke <addr> <val> [sz] - Write value to memory (sz=b/h/w/d, default=d)\n");
    kprintf("  alloc <size>  - Allocate memory of given size\n");
    kprintf("  free <addr>   - Free previously allocated memory\n");
    kprintf("  pmm_info      - Display Physical Memory Manager info\n");
}

void cmd_pmm_info(int argc, char **argv) {
    kprintf("Physical Memory Manager Info:\n");
    kprintf("  Total Usable Memory: %llu KB\n", pmm_get_total_memory() / 1024);
    kprintf("  Free Memory:         %llu KB\n", pmm_get_free_memory() / 1024);
    kprintf("  Highest Usable Addr: 0x%llx\n", pmm_get_highest_usable_address());
}


// --- Shell Main Loop ---

// Table of command handlers
typedef struct {
    const char *name;
    void (*func)(int argc, char **argv);
} command_t;

// Static command array with initialized function pointers
static command_t commands[8];

// Runtime initialization of the command table to work around section initialization issues
static void init_command_table(void) {
    static char help_cmd[] = "help";
    static char memdump_cmd[] = "memdump";
    static char peek_cmd[] = "peek";
    static char poke_cmd[] = "poke";
    static char alloc_cmd[] = "alloc";
    static char free_cmd[] = "free";
    static char pmm_info_cmd[] = "pmm_info";

    commands[0].name = help_cmd;
    commands[0].func = cmd_help;

    commands[1].name = memdump_cmd;
    commands[1].func = cmd_memdump;

    commands[2].name = peek_cmd;
    commands[2].func = cmd_peek;

    commands[3].name = poke_cmd;
    commands[3].func = cmd_poke;

    commands[4].name = alloc_cmd;
    commands[4].func = cmd_alloc;

    commands[5].name = free_cmd;
    commands[5].func = cmd_free;

    commands[6].name = pmm_info_cmd;
    commands[6].func = cmd_pmm_info;

    // Sentinel
    commands[7].name = NULL;
    commands[7].func = NULL;

    // kprintf("Command table initialized:\n");
    // for (int i = 0; i < 7; i++) {
    //     kprintf("  [%d] name at %p: '%s', len=%d\n",
    //             i, commands[i].name, commands[i].name,
    //             strlen(commands[i].name));
    // }
}

void shell_loop() {
    char cmd_buffer[MAX_CMD_LEN];
    char *argv[MAX_ARGS];
    int argc;

    kprintf("\nSolaceOS Shell\n");
    kprintf("Type 'help' for available commands.\n");

    // Initialize command table at runtime
    init_command_table();

    // Debug command table
    // kprintf("Command table at %p:\n", commands);
    // kprintf("Debug: .rodata section address range: %p to %p\n", &_rodata_start, &_rodata_end);
    // for (int i = 0; commands[i].name != NULL; i++) {
    //     kprintf("  [%d] name at %p: '%s', func at %p\n",
    //             i, commands[i].name, commands[i].name, commands[i].func);
    // }

    while (1) {
        asm volatile("svc #0"); // Trigger a syscall for demonstration (can be removed later)
        kprintf("");
        kprintf("\n> ");
        memset(cmd_buffer, 0, MAX_CMD_LEN);
        int i = 0;
        char c;

        // Read command line
        while (i < MAX_CMD_LEN - 1) {
            c = kgetc_blocking(); // Use a blocking read
            if (c == '\r' || c == '\n') {
                kprintf("\n"); // Echo newline
                break;
            } else if (c == '\b' || c == 127) { // Handle backspace
                if (i > 0) {
                    i--;
                    kprintf("\b \b"); // Erase character on screen
                }
            } else if (c >= 32 && c <= 126) { // Printable ASCII
                cmd_buffer[i++] = c;
                kprintf("%c", c); // Echo character
            }
            // Ignore other characters
        }
        cmd_buffer[i] = '\0'; // Null-terminate

        if (i == 0) {
            continue; // Empty command
        }

        // Parse command and arguments using strtok
        argc = 0;
        char *token = strtok(cmd_buffer, " ");
        while (token != NULL && argc < MAX_ARGS) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }

        if (argc == 0) {
            continue; // Only whitespace
        }

        if (strcmp(argv[0], "exit") == 0) {
            kprintf("Exiting shell.\n");
            break; // Exit the shell loop
        }

        // Find and execute command
        bool found = false;

        for (int i = 0; commands[i].name != NULL; i++) {
            if (strcmp(argv[0], commands[i].name) == 0) {
                commands[i].func(argc, argv);
                found = true;
                break;
            }
        }

        if (!found) {
            kprintf("Unknown command: %s\n", argv[0]);
        }
    }
}