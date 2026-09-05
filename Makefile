# Arm-OS Makefile
# Target architecture
ARCH = aarch64

# Cross compiler settings
CROSS_COMPILE = aarch64-linux-gnu-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# Compiler flags
CFLAGS = -Wall -Wextra -Wno-unused-parameter -ffreestanding -nostdlib -nostartfiles -mcpu=cortex-a72 -I./src/include
ASFLAGS = -mcpu=cortex-a72
LDFLAGS = -nostdlib

# Source directories
SRC_DIR = src
BOOT_DIR = $(SRC_DIR)/boot
MEMORY_DIR = $(SRC_DIR)/memory
EXCEPTIONS_DIR = $(SRC_DIR)/exceptions
UI_DIR = $(SRC_DIR)/ui
LIB_DIR = $(SRC_DIR)/lib
SHELL_DIR = $(SRC_DIR)/shell
SYSCALL_DIR = $(SRC_DIR)/syscall
INCLUDE_DIR = $(SRC_DIR)/include

# Build directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Source files
ASM_SRCS = $(wildcard $(BOOT_DIR)/*.S) $(wildcard $(EXCEPTIONS_DIR)/*.S)
C_SRCS = $(wildcard $(BOOT_DIR)/*.c) \
		$(wildcard $(MEMORY_DIR)/*.c) \
		$(wildcard $(EXCEPTIONS_DIR)/*.c) \
		$(wildcard $(UI_DIR)/*.c) \
		$(wildcard $(LIB_DIR)/*.c) \
		$(wildcard $(SHELL_DIR)/*.c) \
		$(wildcard $(SYSCALL_DIR)/*.c)

# Object files
ASM_OBJS = $(patsubst $(SRC_DIR)/%.S, $(OBJ_DIR)/%.o, $(ASM_SRCS))
C_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(C_SRCS))
OBJS = $(ASM_OBJS) $(C_OBJS)

# Output files
KERNEL = $(BUILD_DIR)/kernel8.elf
KERNEL_IMG = $(BUILD_DIR)/kernel8.img

# Targets
.PHONY: all clean qemu debug

all: $(KERNEL_IMG)

$(KERNEL_IMG): $(KERNEL)
	$(OBJCOPY) -O binary $< $@

$(KERNEL): $(OBJS) src/linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -T src/linker.ld -o $@ $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@
	mkdir -p $(OBJ_DIR)/boot
	mkdir -p $(OBJ_DIR)/memory
	mkdir -p $(OBJ_DIR)/exceptions
	mkdir -p $(OBJ_DIR)/ui
	mkdir -p $(OBJ_DIR)/lib
	mkdir -p $(OBJ_DIR)/shell

qemu: $(KERNEL_IMG)
	qemu-system-aarch64 -M virt -cpu cortex-a72 -m 128M -nographic -kernel $(KERNEL_IMG)

debug: $(KERNEL_IMG)
	qemu-system-aarch64 -M virt -cpu cortex-a72 -m 128M -nographic -kernel $(KERNEL_IMG) -S -s

clean:
	rm -rf $(BUILD_DIR)