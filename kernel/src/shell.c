#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "pmm.h"
#include "fs.h"
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

static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static int contains_word(const char *str, const char *word) {
    while (*str) {
        while (*str == ' ') str++;
        if (!*str) break;

        const char *w = word;
        const char *s = str;
        while (*w && *s && to_lower(*s) == to_lower(*w)) {
            s++; w++;
        }
        if (*w == 0 && (*s == 0 || *s == ' ')) return 1;

        while (*str && *str != ' ') str++;
    }
    return 0;
}

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

// Extract the last word as a possible name
static const char *last_word(const char *str) {
    const char *last = str;
    const char *s = str;
    while (*s) {
        if (*s == ' ') last = s + 1;
        s++;
    }
    if (last == str || !*last) return NULL;
    return last;
}

static void print_prompt(void) {
    console_set_color(0x00FFAA);
    console_print("new-os:");
    console_set_color(0x55AAFF);
    fs_print_path();
    console_set_color(0x00FFAA);
    console_print("> ");
    console_set_color(0xFFFFFF);
}

// ========== Commands ==========

static void cmd_help(void) {
    console_set_color(0x55FF55);
    console_print("You can use normal commands or natural language:\n\n");

    console_set_color(0x00FFCC);
    console_print("Filesystem:\n");
    console_set_color(0xCCCCCC);
    console_print("  ls / list / show files     - List current folder\n");
    console_print("  cd <name> / go to <name>   - Change folder\n");
    console_print("  mkdir <name>               - Create folder\n");
    console_print("  create folder <name>       - Create folder (natural)\n\n");

    console_set_color(0x00FFCC);
    console_print("System:\n");
    console_set_color(0xCCCCCC);
    console_print("  help / what can you do     - This help\n");
    console_print("  clear / clear the screen   - Clear screen\n");
    console_print("  mem / show memory          - Memory info\n");
    console_print("  about                      - About New-OS\n\n");
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
    console_print("New-OS v0.6\n");
    console_set_color(0xCCCCCC);
    console_print("A new operating system designed so ordinary people\n");
    console_print("can control the computer with natural language.\n\n");
    console_print("Current features:\n");
    console_print("  - 64-bit kernel + framebuffer console\n");
    console_print("  - Physical memory manager\n");
    console_print("  - Interrupts + keyboard\n");
    console_print("  - Interactive shell\n");
    console_print("  - Basic natural language understanding\n");
    console_print("  - Simple in-memory filesystem\n\n");
}

static void cmd_echo(const char *args) {
    if (args && *args) {
        console_print(args);
        console_print("\n");
    } else {
        console_print("\n");
    }
}

static void cmd_ls(void) {
    console_set_color(0xCCCCCC);
    fs_list();
}

static void cmd_mkdir(const char *name) {
    if (!name || !*name) {
        console_set_color(0xFF5555);
        console_print("Please give a folder name.\n");
        return;
    }

    // Reject common words that are not names
    if (str_eq(name, "folder") || str_eq(name, "directory") ||
        str_eq(name, "a") || str_eq(name, "the") || str_eq(name, "called") ||
        str_eq(name, "named")) {
        console_set_color(0xFF5555);
        console_print("Please give a proper folder name.\n");
        return;
    }

    if (fs_create_folder(name)) {
        console_set_color(0x55FF55);
        console_print("Created folder: ");
        console_print(name);
        console_print("\n");
    } else {
        console_set_color(0xFF5555);
        console_print("Could not create folder (already exists or limit reached).\n");
    }
}

static void cmd_cd(const char *path) {
    if (!path || !*path) {
        console_set_color(0xFF5555);
        console_print("Please specify a folder.\n");
        return;
    }

    if (fs_change_dir(path)) {
        // success - prompt will show new path
    } else {
        console_set_color(0xFF5555);
        console_print("Folder not found: ");
        console_print(path);
        console_print("\n");
    }
}

// ========== Natural Language Engine ==========

static int try_natural_language(const char *line) {
    // Help
    if (contains_word(line, "help") ||
        contains_phrase(line, "what can you do") ||
        contains_phrase(line, "what can i do") ||
        contains_phrase(line, "show commands")) {
        cmd_help();
        return 1;
    }

    // Clear
    if (contains_word(line, "clear") || contains_word(line, "clean") ||
        contains_phrase(line, "clear screen") || contains_phrase(line, "clear the screen")) {
        cmd_clear();
        return 1;
    }

    // Memory
    if (contains_word(line, "memory") || contains_word(line, "mem") ||
        contains_phrase(line, "how much memory") || contains_phrase(line, "show memory") ||
        contains_phrase(line, "memory info") || contains_word(line, "ram")) {
        cmd_mem();
        return 1;
    }

    // About
    if (contains_word(line, "about") || contains_phrase(line, "who are you") ||
        contains_phrase(line, "what is this") || contains_phrase(line, "tell me about")) {
        cmd_about();
        return 1;
    }

    // List files / show files
    if (contains_word(line, "ls") || contains_word(line, "list") ||
        contains_phrase(line, "show files") || contains_phrase(line, "show folders") ||
        contains_phrase(line, "list files") || contains_phrase(line, "list folders") ||
        contains_phrase(line, "what is here") || contains_phrase(line, "whats here")) {
        cmd_ls();
        return 1;
    }

    // Change directory / go to
    if (contains_phrase(line, "go to") || contains_phrase(line, "change to") ||
        contains_phrase(line, "enter folder") || contains_phrase(line, "open folder") ||
        str_starts_with(line, "cd ")) {

        const char *name = NULL;
        if (str_starts_with(line, "cd ")) {
            name = line + 3;
            while (*name == ' ') name++;
        } else {
            name = last_word(line);
        }

        if (name) cmd_cd(name);
        else {
            console_set_color(0xFF5555);
            console_print("Please say which folder.\n");
        }
        return 1;
    }

    // Go up
    if (contains_phrase(line, "go up") || contains_phrase(line, "go back") ||
        contains_phrase(line, "parent folder") || str_eq(line, "cd ..")) {
        cmd_cd("..");
        return 1;
    }

    // Create folder
    if (contains_phrase(line, "create folder") || contains_phrase(line, "make folder") ||
        contains_phrase(line, "make a folder") || contains_phrase(line, "create a folder") ||
        contains_phrase(line, "new folder") || contains_phrase(line, "make directory") ||
        contains_phrase(line, "create directory") || str_starts_with(line, "mkdir ")) {

        const char *name = NULL;

        if (str_starts_with(line, "mkdir ")) {
            name = line + 6;
            while (*name == ' ') name++;
        } else {
            // Look for "called X" or "named X"
            const char *p = line;
            while (*p) {
                if ((to_lower(p[0]) == 'c' && str_starts_with(p, "called ")) ||
                    (to_lower(p[0]) == 'n' && str_starts_with(p, "named "))) {
                    name = p + 7;
                    while (*name == ' ') name++;
                    break;
                }
                p++;
            }

            // Fallback: last word
            if (!name) {
                name = last_word(line);
                if (name && (str_eq(name, "folder") || str_eq(name, "directory") ||
                             str_eq(name, "a") || str_eq(name, "the"))) {
                    name = NULL;
                }
            }
        }

        cmd_mkdir(name);
        return 1;
    }

    return 0;
}

// ========== Command processor ==========

static void process_command(char *line) {
    while (*line == ' ') line++;
    if (*line == 0) return;

    // Traditional exact commands
    if (str_eq(line, "help"))          { cmd_help(); return; }
    if (str_eq(line, "clear"))         { cmd_clear(); return; }
    if (str_eq(line, "mem") || str_eq(line, "memory")) { cmd_mem(); return; }
    if (str_eq(line, "about"))         { cmd_about(); return; }
    if (str_eq(line, "ls") || str_eq(line, "list")) { cmd_ls(); return; }
    if (str_eq(line, "pwd")) {
        console_set_color(0x55AAFF);
        fs_print_path();
        console_print("\n");
        return;
    }

    if (str_starts_with(line, "echo ")) { cmd_echo(line + 5); return; }
    if (str_eq(line, "echo"))          { cmd_echo(""); return; }

    if (str_starts_with(line, "mkdir ")) {
        const char *n = line + 6;
        while (*n == ' ') n++;
        cmd_mkdir(n);
        return;
    }

    if (str_starts_with(line, "cd ")) {
        const char *n = line + 3;
        while (*n == ' ') n++;
        cmd_cd(n);
        return;
    }
    if (str_eq(line, "cd")) {
        cmd_cd("/");
        return;
    }

    // Natural language
    if (try_natural_language(line)) return;

    // Unknown
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
    console_print("Try:  create folder Projects\n");
    console_print("      show files\n");
    console_print("      go to Projects\n\n");

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
