#include "keyboard.h"
#include "console.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static const char scancode_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0,
};

static const char scancode_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','\"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0,
};

static char key_buffer[256];
static volatile size_t key_read = 0;
static volatile size_t key_write = 0;
static int shift_down = 0;
static int caps_lock = 0;

void keyboard_init(void) {
    key_read = 0;
    key_write = 0;
    shift_down = 0;
    caps_lock = 0;
}

void keyboard_handler(void) {
    uint8_t sc = inb(0x60);

    // Shift press/release
    if (sc == 0x2A || sc == 0x36) { shift_down = 1; return; }
    if (sc == 0xAA || sc == 0xB6) { shift_down = 0; return; }

    // Caps lock toggle
    if (sc == 0x3A) { caps_lock = !caps_lock; return; }

    if (sc & 0x80) return; // key release

    char c = 0;
    if (sc < 128) {
        if (shift_down)
            c = scancode_shift[sc];
        else
            c = scancode_ascii[sc];

        // Caps lock affects letters only
        if (caps_lock && c >= 'a' && c <= 'z') c -= 32;
        else if (caps_lock && c >= 'A' && c <= 'Z' && !shift_down) c += 32;
    }

    if (c != 0) {
        size_t next = (key_write + 1) % 256;
        if (next != key_read) {
            key_buffer[key_write] = c;
            key_write = next;
        }
    }
}

bool keyboard_has_char(void) {
    return key_read != key_write;
}

char keyboard_getchar(void) {
    while (!keyboard_has_char()) asm volatile ("hlt");
    char c = key_buffer[key_read];
    key_read = (key_read + 1) % 256;
    return c;
}
