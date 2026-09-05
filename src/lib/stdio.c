#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "lib/stdio.h"
#include "lib/uart.h"
#include "lib/string.h"

// Buffer sizes
#define PRINTF_BUFFER_SIZE 1024
#define MAX_INT_DIGITS 21  // For 64-bit integer

// Forward declarations for printf helper functions
static void print_string(const char *str);
static void print_char(char c);
static void print_int(int64_t value, int base, bool is_signed);
static void print_uint(uint64_t value, int base);
static void print_hex(uint64_t value, bool uppercase);
static void print_pointer(void *ptr);

// Basic kprintf implementation
int kprintf(const char *format, ...) {
    char buffer[PRINTF_BUFFER_SIZE];
    char *buf_ptr = buffer;
    va_list args;
    va_start(args, format);
    
    while (*format != '\0' && (size_t)(buf_ptr - buffer) < PRINTF_BUFFER_SIZE - 1) {
        if (*format == '%') {
            format++;
            
            // Check for length modifiers
            bool is_long = false;
            bool is_longlong = false;
            bool is_size_t = false;

            // Skip printf flags and field width before reading the length
            // modifier. Padding is optional for this freestanding formatter.
            while (*format == '-' || *format == '+' || *format == '0' ||
                   *format == ' ') {
                format++;
            }
            while (*format >= '0' && *format <= '9') {
                format++;
            }
            
            if (*format == 'l') {
                is_long = true;
                format++;
                if (*format == 'l') {
                    is_longlong = true;
                    is_long = false;
                    format++;
                }
            } else if (*format == 'z') {
                is_size_t = true;
                format++;
            }
            
            // Process format specifier
            switch (*format) {
                case 'c':
                    *buf_ptr++ = (char)va_arg(args, int);
                    break;
                case 's': {
                    const char *str = va_arg(args, const char*);
                    if (str == NULL) str = "(null)";
                    size_t len = strlen(str);
                    if (len > PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1)
                        len = PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1;
                    memcpy(buf_ptr, str, len);
                    buf_ptr += len;
                    break;
                }
                case 'd':
                case 'i': {
                    int64_t value;
                    if (is_longlong)
                        value = va_arg(args, int64_t);
                    else if (is_long || is_size_t)
                        value = va_arg(args, long);
                    else
                        value = va_arg(args, int);
                    
                    // Convert to string using helper function
                    char num_buf[MAX_INT_DIGITS];
                    char *num_ptr = num_buf + MAX_INT_DIGITS - 1;
                    *num_ptr = '\0';
                    
                    bool is_negative = value < 0;
                    uint64_t abs_value = is_negative ? -value : value;
                    
                    do {
                        *--num_ptr = '0' + (abs_value % 10);
                        abs_value /= 10;
                    } while (abs_value > 0);
                    
                    if (is_negative) {
                        *--num_ptr = '-';
                    }
                    
                    size_t len = strlen(num_ptr);
                    if (len > PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1)
                        len = PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1;
                    memcpy(buf_ptr, num_ptr, len);
                    buf_ptr += len;
                    break;
                }
                case 'u': {
                    uint64_t value;
                    if (is_longlong)
                        value = va_arg(args, uint64_t);
                    else if (is_long || is_size_t)
                        value = va_arg(args, unsigned long);
                    else
                        value = va_arg(args, unsigned int);
                    
                    // Convert to string
                    char num_buf[MAX_INT_DIGITS];
                    char *num_ptr = num_buf + MAX_INT_DIGITS - 1;
                    *num_ptr = '\0';
                    
                    do {
                        *--num_ptr = '0' + (value % 10);
                        value /= 10;
                    } while (value > 0);
                    
                    size_t len = strlen(num_ptr);
                    if (len > PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1)
                        len = PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1;
                    memcpy(buf_ptr, num_ptr, len);
                    buf_ptr += len;
                    break;
                }
                case 'x':
                case 'X': {
                    uint64_t value;
                    if (is_longlong)
                        value = va_arg(args, uint64_t);
                    else if (is_long || is_size_t)
                        value = va_arg(args, unsigned long);
                    else
                        value = va_arg(args, unsigned int);
                    
                    // Convert to hex string
                    char num_buf[MAX_INT_DIGITS];
                    char *num_ptr = num_buf + MAX_INT_DIGITS - 1;
                    *num_ptr = '\0';
                    
                    const char *hex_chars = (*format == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
                    
                    do {
                        *--num_ptr = hex_chars[value & 0xF];
                        value >>= 4;
                    } while (value > 0);
                    
                    size_t len = strlen(num_ptr);
                    if (len > PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1)
                        len = PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1;
                    memcpy(buf_ptr, num_ptr, len);
                    buf_ptr += len;
                    break;
                }
                case 'p': {
                    void *ptr = va_arg(args, void*);
                    
                    // Print "0x" prefix
                    if (buf_ptr + 2 < buffer + PRINTF_BUFFER_SIZE - 1) {
                        *buf_ptr++ = '0';
                        *buf_ptr++ = 'x';
                    }
                    
                    // Convert to hex string
                    uint64_t value = (uint64_t)ptr;
                    char num_buf[MAX_INT_DIGITS];
                    char *num_ptr = num_buf + MAX_INT_DIGITS - 1;
                    *num_ptr = '\0';
                    
                    // Always print full pointer width (16 hex digits for 64-bit)
                    int digit_count = 0;
                    do {
                        *--num_ptr = "0123456789abcdef"[value & 0xF];
                        value >>= 4;
                        digit_count++;
                    } while (value > 0 || digit_count < 16);
                    
                    size_t len = strlen(num_ptr);
                    if (len > PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1)
                        len = PRINTF_BUFFER_SIZE - (buf_ptr - buffer) - 1;
                    memcpy(buf_ptr, num_ptr, len);
                    buf_ptr += len;
                    break;
                }
                case '%':
                    *buf_ptr++ = '%';
                    break;
                default:
                    // Unsupported format specifier, just copy it
                    *buf_ptr++ = '%';
                    *buf_ptr++ = *format;
                    break;
            }
        } else {
            *buf_ptr++ = *format;
        }
        
        format++;
    }
    
    // Null-terminate the buffer
    *buf_ptr = '\0';
    
    // Output the buffer through UART
    char *output_ptr = buffer;
    while (*output_ptr) {
        uart_putc(*output_ptr++);
    }
    
    va_end(args);
    return buf_ptr - buffer;
}

// Get a character (non-blocking)
char kgetc(void) {
    return uart_getc();
}

// Simple blocking character read
char kgetc_blocking(void) {
    char c;
    while ((c = uart_getc()) == 0) {
        // Wait for character
        asm volatile("yield");
    }
    
    // Convert CR (Enter key) to LF for processing
    if (c == '\r') {
        return '\n';
    }
    
    return c;
}