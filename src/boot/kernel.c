#include "kernel.h"
#include "lib/stdio.h"

// External functions we'll implement later
extern void frame_alloc_init(const KERNEL_BOOT_PARAMS *params);
extern void kheap_init(void);
extern int tui_init(void);
extern void shell_loop(void);

// Debug function called from boot code before UART is initialized
// We need a direct hardware access version
static void early_debug_print(const char *str) {
    // UART direct access (PL011)
    volatile uint32_t *uart_dr = (volatile uint32_t*)0x09000000;
    volatile uint32_t *uart_fr = (volatile uint32_t*)0x09000018;
    
    while (*str) {
        // Wait for FIFO to have space
        while ((*uart_fr) & (1 << 5)); // TXFF bit
        
        // Send character
        *uart_dr = (uint32_t)*str++;
    }
}

// Debug function for section copying, called from assembly
void boot_debug_copy(void *dest, void *src, size_t size) {
    // Print directly using early UART access
    early_debug_print("[BOOT] Copying section: ");
    
    // Convert size to string manually (very simple)
    char size_str[16];
    int i = 0;
    int tmp = (int)size;
    
    // Handle special case of zero
    if (tmp == 0) {
        size_str[0] = '0';
        size_str[1] = '\0';
    } else {
        // Convert integer to string (backwards)
        while (tmp > 0) {
            size_str[i++] = '0' + (tmp % 10);
            tmp /= 10;
        }
        size_str[i] = '\0';
        
        // Reverse the string
        for (int j = 0; j < i/2; j++) {
            char c = size_str[j];
            size_str[j] = size_str[i-j-1];
            size_str[i-j-1] = c;
        }
    }
    
    early_debug_print(size_str);
    early_debug_print(" bytes\n");
}

void shutdown(void) {
    // Ask the platform firmware to power off the virtual machine.
    register uint64_t psci_function asm("x0") = 0x84000008;
    asm volatile("hvc #0" : "+r"(psci_function) :: "memory");

    // If the above doesn't work, halt the CPU
    kprintf("Shutdown failed, halting CPU.\n");
    while (1) {
        asm volatile("wfi");
    }
}

// Kernel entry point
void kernel_main(KERNEL_BOOT_PARAMS *params) {
    // Early initialization - placeholder for UART setup
    // We'll assume kprintf writes to a UART for now
    
    kprintf("SolaceOS starting...\n");
    // kprintf("Kernel loaded at physical address: 0x%llx\n", 
    //         params ? params->kernel_phys_start : 0);
            
    // Debug section information
    // kprintf("Memory Sections:\n");
    // kprintf("  .text:   %p to %p\n", &_kernel_start, &_text_end);
    // kprintf("  .rodata: %p to %p (load: %p)\n", &_rodata_start, &_rodata_end, &_rodata_load);
    // kprintf("  .data:   %p to %p (load: %p)\n", &_data_start, &_data_end, &_data_load);
    // kprintf("  .bss:    %p to %p\n", &_bss_start, &_bss_end);
    
    // Initialize memory management subsystem
    kprintf("Initializing Physical Memory Manager...\n");
    frame_alloc_init(params);
    
    kprintf("Initializing Kernel Heap Allocator...\n");
    kheap_init();
    
    // Initialize Text User Interface
    kprintf("Initializing TUI subsystem...\n");
    if (tui_init() != 0) {
        kprintf("Failed to initialize TUI subsystem!\n");
        // Continue without TUI for now
    }
    
    // Enter the shell
    kprintf("Starting shell...\n");
    shell_loop();
    
    // If shell returns, halt
    kprintf("Kernel halting.\n");
    
    shutdown();
}