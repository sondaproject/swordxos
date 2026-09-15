//SwordXOS kernel, i will add a kernel name in future updates cuz i like naming things :-)
//quick thing, dont use default gcc, INSTEAD use the i686 gcc compiler.
//version 003 added reboot, echo with arguments, and bug fixes

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) 
{
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
    return (uint16_t) uc | (uint16_t) color << 8;
}

size_t strlen(const char* str) 
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000 

// Read a byte from an I/O port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Write a byte to an I/O port (needed for reboot)
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

// Basic US Keyboard Scancode Set 1 lookup table
const char scancode_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

// Poll the PS/2 keyboard for a character
char keyboard_read_char(void) {
    while (1) {
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            if (!(scancode & 0x80)) {
                return scancode_ascii[scancode];
            }
        }
    }
}

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;

void terminal_initialize(void) 
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_setcolor(uint8_t color) 
{
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

void terminal_scroll(void) 
{
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[(y - 1) * VGA_WIDTH + x] = terminal_buffer[y * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c) 
{
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
        return;
    }

    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
    }
}

void terminal_write(const char* data, size_t size) 
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) 
{
    terminal_write(data, strlen(data));
}

// Reboot command implementation
void reboot_system(void) {
    terminal_writestring("Rebooting system...\n");
    outb(0x64, 0xFE);
}

bool strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 == *(const unsigned char*)s2;
}

// Helper function to check command prefixes for echo
bool starts_with(const char* pre, const char* str) {
    size_t i = 0;
    while (pre[i] != '\0') {
        if (str[i] != pre[i]) return false;
        i++;
    }
    return true;
}

void launch_shell(void) {
    char input_buffer[256];
    size_t buf_index = 0;

    terminal_writestring("shell$ ");

    while (true) {
        char c = keyboard_read_char();

        if (c == '\n') {
            terminal_putchar('\n');
            input_buffer[buf_index] = '\0';

            if (buf_index > 0) {
                if (strcmp(input_buffer, "help")) {
                    terminal_writestring("Available commands:\n");
                    terminal_writestring("  help         - I think its pretty clear what it does\n");
                    terminal_writestring("  about        - Display OS info\n");
                    terminal_writestring("  clear        - Clear the screen\n");
                    terminal_writestring("  version      - Display OS version\n");
                    terminal_writestring("  echo [text]  - Prints text on the terminal\n");
                    terminal_writestring("  reboot       - Restart the computer\n");
                } else if (strcmp(input_buffer, "about")) {
                    terminal_writestring("SwordXOS, linux remade from scratch i need a better name\n");
                } else if (strcmp(input_buffer, "clear")) {
                    terminal_initialize();
                } else if (strcmp(input_buffer, "version")) {
                    terminal_writestring("version 0.0.1 (003)\n");
                } else if (starts_with("echo ", input_buffer)) {
                    terminal_writestring(input_buffer + 5);
                    terminal_writestring("\n");
                } else if (strcmp(input_buffer, "reboot")) {
                    reboot_system();
                } else {
                    terminal_writestring("Unknown command. are you speaking arabic or something?.\n");
                }
            }

            buf_index = 0;
            terminal_writestring("shell$ ");
        } 
        else if (c == '\b') {
            if (buf_index > 0) {
                buf_index--;
                terminal_column--;
                terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
            }
        } 
        else if (c != 0 && buf_index < sizeof(input_buffer) - 1) {
            input_buffer[buf_index++] = c;
            terminal_putchar(c);
        }
    }
}

void kernel_main(void) 
{
    terminal_initialize();

    terminal_writestring("SwordXOS Booted with GRUB\n");
    
    launch_shell();
}
