#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "pmm.h"
#include <stddef.h>
#include <stdint.h>

#define INPUT_MAX 128

static char input_buffer[INPUT_MAX];
static size_t input_len = 0;

// ========== String helpers ==========

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

// Case-insensitive character compare
static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

// Check if string contains a word (case-insensitive, whole word)
static int contains_word(const char *str, const char *word) {
    while (*str) {
        // Skip to start of word
        while (*str == ' ') str++;
        if (!*str) break;

        const char *w = word;
        const char *s = str;
        while (*w && *s && to_lower(*s) == to_lower(*w)) {
            s++; w++;
        }
        if (*w == 0 && (*s == 0 || *s == ' ')) {
            return 1; // found whole word
        }

        // Move to next word
        while (*str && *str != ' ') str++;
    }
    return 0;
}

// Check if string contains a phrase (case-insensitive)
static int contains_phrase(const char *str, const char *phrase) {
    while (*str) {
        const char *p = phrase;
        const char *s = str;
        while (*p && *s && to_lower(*s) == to_lower(*p)) {
            s++; p++;
        }
        if (*p == 0) return 1;
        str++;
    }
    return 0;
}

static void print_prompt(void) {
    console_set_color(0x00FFAA);
    console_print("new-os> ");
    console_set_color(0xFFFFFF);
}

// ========== Commands ==========

static void cmd_help(void) {
    console_set_color(0x55FF55);
    console_print("You can use normal commands or natural language:\n\n");

    console_set_color(0x00FFCC);
    console_print("Traditional commands:\n");
    console_set_color(0xCCCCCC);
    console_print("  help, clear, mem, echo, about\n\n");

    console_set_color(0x00FFCC);
    console_print("Natural language examples:\n");
    console_set_color(0xCCCCCC);
    console_print("  show memory / how much memory\n");
    console_print("  clear the screen / clean screen\n");
    console_print("  help me / what can you do\n");
    console_print("  tell me about this system\n");
    console_print("  create folder Documents\n");
    console_print("  make a folder called Photos\n\n");
}

static void cmd_clear(void) {
    console_clear(0x0A0A1A);
}

static void cmd_mem(void) {
    console_set_color(0xAAAAFF);
    console_print("Memory Information:\n");
    console_set_color(0xCCCCCC);

    console_print("  Total usable : ");
    console_print_dec(pmm_get_total_memory() / 1024 / 1024);
    console_print(" MiB\n");

    console_print("  Free         : ");
    console_print_dec(pmm_get_free_memory() / 1024 / 1024);
    console_print(" MiB\n");

    console_print("  Used         : ");
    console_print_dec(pmm_get_used_memory() / 1024 / 1024);
    console_print(" MiB\n\n");
}

static void cmd_about(void) {
    console_set_color(0x00FFCC);
    console_print("New-OS v0.5\n");
    console_set_color(0xCCCCCC);
    console_print("A new operating system designed so ordinary people\n");
    console_print("can control the computer with natural language.\n\n");
    console_print("Current features:\n");
    console_print("  - 64-bit kernel\n");
    console_print("  - Framebuffer console\n");
    console_print("  - Physical memory manager\n");
    console_print("  - Interrupts + keyboard\n");
    console_print("  - Interactive shell\n");
    console_print("  - Basic natural language understanding\n\n");
}

static void cmd_echo(const char *args) {
    if (args && *args) {
        console_print(args);
        console_print("\n");
    } else {
        console_print("\n");
    }
}

// Placeholder for future filesystem
static void cmd_create_folder(const char *name) {
    console_set_color(0xFFCC55);
    console_print("I understood: create folder");
    if (name && *name) {
        console_print(" named '");
        console_print(name);
        console_print("'");
    }
    console_print("\n");
    console_set_color(0xAAAAAA);
    console_print("(Filesystem not implemented yet — this is a preview)\n\n");
}

// ========== Natural Language Engine ==========

// Try to understand natural language. Returns 1 if handled, 0 if not.
static int try_natural_language(const char *line) {
    // --- Help related ---
    if (contains_word(line, "help") ||
        contains_phrase(line, "what can you do") ||
        contains_phrase(line, "what can i do") ||
        contains_phrase(line, "how do i") ||
        contains_phrase(line, "show commands")) {
        cmd_help();
        return 1;
    }

    // --- Clear screen ---
    if (contains_word(line, "clear") ||
        contains_word(line, "clean") ||
        contains_phrase(line, "clear screen") ||
        contains_phrase(line, "clear the screen") ||
        contains_phrase(line, "clean screen")) {
        cmd_clear();
        return 1;
    }

    // --- Memory ---
    if (contains_word(line, "memory") ||
        contains_word(line, "mem") ||
        contains_phrase(line, "how much memory") ||
        contains_phrase(line, "show memory") ||
        contains_phrase(line, "memory info") ||
        contains_phrase(line, "memory information") ||
        contains_phrase(line, "ram")) {
        cmd_mem();
        return 1;
    }

    // --- About ---
    if (contains_word(line, "about") ||
        contains_phrase(line, "who are you") ||
        contains_phrase(line, "what is this") ||
        contains_phrase(line, "tell me about")) {
        cmd_about();
        return 1;
    }

    // --- Create folder (preview) ---
    if (contains_phrase(line, "create folder") ||
        contains_phrase(line, "make folder") ||
        contains_phrase(line, "make a folder") ||
        contains_phrase(line, "create a folder") ||
        contains_phrase(line, "new folder") ||
        contains_phrase(line, "make directory") ||
        contains_phrase(line, "create directory")) {

        // Try to extract a name after "called" or "named" or at the end
        const char *name = 0;

        // Look for "called X" or "named X"
        const char *p = line;
        while (*p) {
            if ((to_lower(p[0]) == 'c' && str_starts_with(p, "called ")) ||
                (to_lower(p[0]) == 'n' && str_starts_with(p, "named "))) {
                name = p + 7; // skip "called " or "named "
                while (*name == ' ') name++;
                break;
            }
            p++;
        }

        // If no "called/named", take the last word as name
        if (!name) {
            // Find last space
            const char *last = line;
            const char *s = line;
            while (*s) {
                if (*s == ' ') last = s + 1;
                s++;
            }
            // Only use last word if it's not one of the command words
            if (last != line &&
                !str_eq(last, "folder") &&
                !str_eq(last, "directory") &&
                !str_eq(last, "a")) {
                name = last;
            }
        }

        cmd_create_folder(name);
        return 1;
    }

    return 0; // not understood as natural language
}

// ========== Command processor ==========

static void process_command(char *line) {
    // Skip leading spaces
    while (*line == ' ') line++;

    if (*line == 0) return; // empty line

    // 1. Try traditional exact commands first
    if (str_eq(line, "help")) {
        cmd_help();
        return;
    }
    if (str_eq(line, "clear")) {
        cmd_clear();
        return;
    }
    if (str_eq(line, "mem") || str_eq(line, "memory")) {
        cmd_mem();
        return;
    }
    if (str_eq(line, "about")) {
        cmd_about();
        return;
    }
    if (str_starts_with(line, "echo ")) {
        cmd_echo(line + 5);
        return;
    }
    if (str_eq(line, "echo")) {
        cmd_echo("");
        return;
    }

    // 2. Try natural language understanding
    if (try_natural_language(line)) {
        return;
    }

    // 3. Unknown
    console_set_color(0xFF5555);
    console_print("I don't understand: \"");
    console_print(line);
    console_print("\"\n");
    console_set_color(0xAAAAAA);
    console_print("Type 'help' or 'what can you do' for options.\n\n");
}

void shell_run(void) {
    console_set_color(0x00FFCC);
    console_print("========================================\n");
    console_print("       New-OS Interactive Shell\n");
    console_print("========================================\n\n");
    console_set_color(0xCCCCCC);
    console_print("You can type normal commands or natural language.\n");
    console_print("Examples:  help   |   show memory   |   clear screen\n\n");

    print_prompt();

    for (;;) {
        if (!keyboard_has_char()) {
            asm volatile ("hlt");
            continue;
        }

        char c = keyboard_getchar();

        if (c == '\n') {
            console_print("\n");
            input_buffer[input_len] = 0;
            process_command(input_buffer);
            input_len = 0;
            print_prompt();
        } else if (c == '\b') {
            if (input_len > 0) {
                input_len--;
                console_print("\b \b");
            }
        } else if (c >= 32 && c < 127) {
            if (input_len < INPUT_MAX - 1) {
                input_buffer[input_len++] = c;
                char buf[2] = {c, 0};
                console_print(buf);
            }
        }
    }
}
