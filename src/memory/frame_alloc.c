#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "memory/frame_alloc.h"
#include "lib/string.h"
#include "lib/stdio.h"

// For QEMU virt, RAM often starts at 0x40000000 and can be e.g., 128MB or more.
// Let's assume a max manageable physical address space, e.g., 1GB beyond RAM start.
// Adjust this based on actual QEMU configuration or dynamic detection.
#define PMM_MANAGEABLE_SIZE (1024ULL * 1024ULL * 1024ULL) // Manage up to 1GB
#define PMM_MAX_ADDRESS (PMM_RAM_BASE + PMM_MANAGEABLE_SIZE)
#define PMM_TOTAL_FRAMES (PMM_MANAGEABLE_SIZE / PAGE_SIZE)

// Bitmap for tracking frame usage. Each bit represents one frame.
// Size = Total Frames / 8 bits per byte
static uint8_t *frame_bitmap = &_pmm_bitmap_start;

static uint64_t total_memory = 0;
static uint64_t free_memory = 0;
static uint64_t highest_usable_address = 0;

// Helper function to set a bit in the bitmap
static void set_bit(size_t bit) {
    frame_bitmap[bit / 8] |= (1 << (bit % 8));
}

// Helper function to clear a bit in the bitmap
static void clear_bit(size_t bit) {
    frame_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

// Helper function to test a bit in the bitmap
static bool test_bit(size_t bit) {
    return (frame_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

// Mark a range of frames as used
static void mark_range_used(uint64_t base_addr, uint64_t size) {
    uint64_t start_frame = (base_addr >= PMM_RAM_BASE) ? 
                          (base_addr - PMM_RAM_BASE) / PAGE_SIZE : 
                          (UINT64_MAX);
    uint64_t end_addr = base_addr + size;
    uint64_t end_frame = (end_addr > PMM_RAM_BASE) ? 
                        (end_addr - 1 - PMM_RAM_BASE) / PAGE_SIZE : 
                        0;

    if (start_frame >= PMM_TOTAL_FRAMES) return; // Start address out of managed range
    if (end_frame >= PMM_TOTAL_FRAMES) end_frame = PMM_TOTAL_FRAMES - 1; // Cap end address

    // kprintf("PMM: Marking used 0x%llx - 0x%llx (Frames %llu - %llu)\n",
    //         base_addr, base_addr + size, start_frame, end_frame);

    for (uint64_t i = start_frame; i <= end_frame; ++i) {
        if (!test_bit(i)) {
            set_bit(i);
            // Assuming these were initially counted as free, decrement count
            if (total_memory >= PAGE_SIZE) total_memory -= PAGE_SIZE;
            if (free_memory >= PAGE_SIZE) free_memory -= PAGE_SIZE;
        }
    }
}

// Mark a range of frames as free
static void mark_range_free(uint64_t base_addr, uint64_t size) {
    uint64_t start_frame = (base_addr >= PMM_RAM_BASE) ? 
                          (base_addr - PMM_RAM_BASE) / PAGE_SIZE : 
                          (UINT64_MAX);
    uint64_t end_addr = base_addr + size;
    uint64_t end_frame = (end_addr > PMM_RAM_BASE) ? 
                        (end_addr - 1 - PMM_RAM_BASE) / PAGE_SIZE : 
                        0;

    if (start_frame >= PMM_TOTAL_FRAMES) return; // Start address out of managed range
    if (end_frame >= PMM_TOTAL_FRAMES) end_frame = PMM_TOTAL_FRAMES - 1; // Cap end address

    // kprintf("PMM: Marking free 0x%llx - 0x%llx (Frames %llu - %llu)\n",
    //         base_addr, base_addr + size, start_frame, end_frame);

    for (uint64_t i = start_frame; i <= end_frame; ++i) {
        if (test_bit(i)) { // Only count if it wasn't already free
             total_memory += PAGE_SIZE;
             free_memory += PAGE_SIZE;
             if ((PMM_RAM_BASE + (i + 1) * PAGE_SIZE) > highest_usable_address) {
                 highest_usable_address = PMM_RAM_BASE + (i + 1) * PAGE_SIZE;
             }
        }
       clear_bit(i); // Mark as free regardless
    }
}

void frame_alloc_init(const KERNEL_BOOT_PARAMS *params) {
    kprintf("PMM: Initializing Physical Memory Manager...\n");
    
    // Calculate the bitmap size
    size_t bitmap_size = (&_pmm_bitmap_end - &_pmm_bitmap_start);
    kprintf("PMM: Bitmap size: %zu bytes, located at %p\n", 
            bitmap_size, frame_bitmap);
    
    // Initially, mark all manageable frames as used
    memset(frame_bitmap, 0xFF, bitmap_size);
    total_memory = 0;
    free_memory = 0;
    highest_usable_address = PMM_RAM_BASE;
    
    if (params) {
        // kprintf("PMM: Kernel Physical Range: 0x%llx - 0x%llx\n",
        //         params->kernel_phys_start, params->kernel_phys_end);
                
        // For now, we'll simplify and just reserve the kernel address range
        // In a full implementation, we would parse the UEFI memory map from params
        
        // Mark all memory as free initially (simplified approach)
        mark_range_free(PMM_RAM_BASE, PMM_MANAGEABLE_SIZE);
        
        // Then mark kernel memory as used
        mark_range_used(params->kernel_phys_start, 
                       params->kernel_phys_end - params->kernel_phys_start);
    } else {
        // No boot parameters, use linker-provided kernel boundaries
        uint64_t kernel_start = (uint64_t)&_kernel_start;
        uint64_t kernel_end = (uint64_t)&_kernel_end;
        
        // kprintf("PMM: Kernel boundaries from linker: 0x%llx - 0x%llx\n",
        //         kernel_start, kernel_end);
                
        // Mark all memory as free initially
        mark_range_free(PMM_RAM_BASE, PMM_MANAGEABLE_SIZE);
        
        // Then mark kernel memory as used
        mark_range_used(kernel_start, kernel_end - kernel_start);
    }
    
    // Mark the bitmap itself as used (it lies within kernel memory, but just to be explicit)
    mark_range_used((uint64_t)frame_bitmap, bitmap_size);
    
    kprintf("PMM: Initialization complete. Total: %llu KB, Free: %llu KB\n",
            total_memory / 1024, free_memory / 1024);
}

void* alloc_frame(void) {
    // Simple linear search for first free frame
    for (size_t i = 0; i < PMM_TOTAL_FRAMES; i++) {
        if (!test_bit(i)) {
            set_bit(i);
            free_memory -= PAGE_SIZE;
            
            // Calculate the physical address
            void *frame_addr = (void*)(PMM_RAM_BASE + i * PAGE_SIZE);
            
            // Zero the frame for security/predictability
            memset(frame_addr, 0, PAGE_SIZE);
            
            return frame_addr;
        }
    }
    
    kprintf("PMM: ERROR - Out of physical frames!\n");
    return NULL; // No free frame found
}

void free_frame(void *frame) {
    if (!frame) return;
    
    uint64_t addr = (uint64_t)frame;
    
    // Basic validation
    if (addr < PMM_RAM_BASE || addr >= PMM_MAX_ADDRESS) {
        kprintf("PMM: Attempt to free invalid frame at %p\n", frame);
        return;
    }
    
    // Check alignment
    if (addr % PAGE_SIZE != 0) {
        kprintf("PMM: Attempt to free unaligned address %p\n", frame);
        return;
    }
    
    // Calculate the bit index
    size_t frame_idx = (addr - PMM_RAM_BASE) / PAGE_SIZE;
    
    if (frame_idx >= PMM_TOTAL_FRAMES) {
        kprintf("PMM: Frame index %zu out of range\n", frame_idx);
        return;
    }
    
    // Check if frame is currently marked as used
    if (!test_bit(frame_idx)) {
        kprintf("PMM: Warning - double free detected for frame %p\n", frame);
        return;
    }
    
    // Mark as free
    clear_bit(frame_idx);
    free_memory += PAGE_SIZE;
}

uint64_t pmm_get_total_memory(void) {
    return total_memory;
}

uint64_t pmm_get_free_memory(void) {
    return free_memory;
}

uint64_t pmm_get_highest_usable_address(void) {
    return highest_usable_address;
}