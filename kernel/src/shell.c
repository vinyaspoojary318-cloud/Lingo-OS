#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "pmm.h"
#include "fs.h"
#include "process.h"
#include <stddef.h>
#include <stdint.h>

#define INPUT_MAX 128

static char input_buffer[INPUT_MAX];
static size_t input_len = 0;

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
        while (*w && *s && to_lower(*s) == to_lower(*w)) { s++; w++; }
        if (*w == 0 && (*s == 0 || *s == ' ')) return 1;
        while (*str && *str != ' ') str++;
    }
    return 0;
}

static int contains_phrase(const char *str, const char *phrase) {
    while (*str) {
        const char *p = phrase;
        const char *s = str;
        while (*p && *s && to_lower(*s) == to_lower(*p)) { s++; p++; }
        if (*p == 0) return 1;
        str++;
    }
    return 0;
}

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

static void cmd_help(void) {
    console_set_color(0x55FF55);
    console_print("Commands & natural language:\n\n");
    console_set_color(0x00FFCC);
    console_print("Filesystem:\n");
    console_set_color(0xCCCCCC);
    console_print("  ls / show files / create folder / create file\n");
    console_print("  delete / go to / read\n\n");
    console_set_color(0x00FFCC);
    console_print("Processes:\n");
    console_set_color(0xCCCCCC);
    console_print("  ps / show processes     - List processes\n");
    console_print("  create process <name>   - Create a new process\n\n");
    console_set_color(0x00FFCC);
    console_print("System:\n");
    console_set_color(0xCCCCCC);
    console_print("  help / clear / mem / about\n\n");
}

static void cmd_clear(void) { console_clear(0x0A0A1A); }

static void cmd_mem(void) {
    console_set_color(0xAAAAFF);
    console_print("Memory Information:\n");
    console_set_color(0xCCCCCC);
    console_print("  Total usable : ");
    console_print_dec(pmm_get_total_memory() / 1024 / 1024);
    console_print(" MiB\n  Free         : ");
    console_print_dec(pmm_get_free_memory() / 1024 / 1024);
    console_print(" MiB\n  Used         : ");
    console_print_dec(pmm_get_used_memory() / 1024 / 1024);
    console_print(" MiB\n\n");
}

static void cmd_about(void) {
    console_set_color(0x00FFCC);
    console_print("New-OS v0.9\n");
    console_set_color(0xCCCCCC);
    console_print("Natural-language-first OS.\n");
    console_print("Now with process structures (dual-mode foundation).\n\n");
}

static void cmd_echo(const char *args) {
    if (args && *args) { console_print(args); console_print("\n"); }
    else console_print("\n");
}

static void cmd_ls(void) { console_set_color(0xCCCCCC); fs_list(); }

static void cmd_mkdir(const char *name) {
    if (!name || !*name) {
        console_set_color(0xFF5555);
        console_print("Please give a folder name.\n");
        return;
    }
    if (fs_create_folder(name)) {
        console_set_color(0x55FF55);
        console_print("Created folder: "); console_print(name); console_print("\n");
    } else {
        console_set_color(0xFF5555);
        console_print("Could not create folder.\n");
    }
}

static void cmd_create_file(const char *name) {
    if (!name || !*name) {
        console_set_color(0xFF5555);
        console_print("Please give a file name.\n");
        return;
    }
    if (fs_create_file(name, "")) {
        console_set_color(0x55FF55);
        console_print("Created file: "); console_print(name); console_print("\n");
    } else {
        console_set_color(0xFF5555);
        console_print("Could not create file.\n");
    }
}

static void cmd_delete(const char *name) {
    if (!name || !*name) {
        console_set_color(0xFF5555);
        console_print("Please give a name to delete.\n");
        return;
    }
    if (fs_delete(name)) {
        console_set_color(0x55FF55);
        console_print("Deleted: "); console_print(name); console_print("\n");
    } else {
        console_set_color(0xFF5555);
        console_print("Could not delete.\n");
    }
}

static void cmd_read(const char *name) {
    if (!name || !*name) {
        console_set_color(0xFF5555);
        console_print("Please give a file name.\n");
        return;
    }
    char buf[256];
    if (fs_read_file(name, buf, sizeof(buf))) {
        console_set_color(0xCCCCCC);
        if (buf[0] == 0) console_print("(empty file)\n");
        else { console_print(buf); console_print("\n"); }
    } else {
        console_set_color(0xFF5555);
        console_print("File not found or is a folder.\n");
    }
}

static void cmd_cd(const char *path) {
    if (!path || !*path) {
        console_set_color(0xFF5555);
        console_print("Please specify a folder.\n");
        return;
    }
    if (!fs_change_dir(path)) {
        console_set_color(0xFF5555);
        console_print("Folder not found: "); console_print(path); console_print("\n");
    }
}

static void cmd_ps(void) {
    process_list();
}

static void cmd_create_process(const char *name) {
    if (!name || !*name) {
        console_set_color(0xFF5555);
        console_print("Please give a process name.\n");
        return;
    }
    int pid = process_create(name);
    if (pid >= 0) {
        console_set_color(0x55FF55);
        console_print("Created process '");
        console_print(name);
        console_print("' with PID ");
        console_print_dec(pid);
        console_print("\n");
    } else {
        console_set_color(0xFF5555);
        console_print("Failed to create process.\n");
    }
}

static int try_natural_language(const char *line) {
    if (contains_word(line, "help") || contains_phrase(line, "what can you do")) {
        cmd_help(); return 1;
    }
    if (contains_word(line, "clear") || contains_phrase(line, "clear the screen")) {
        cmd_clear(); return 1;
    }
    if (contains_word(line, "memory") || contains_word(line, "mem") ||
        contains_phrase(line, "show memory")) {
        cmd_mem(); return 1;
    }
    if (contains_word(line, "about")) { cmd_about(); return 1; }

    if (contains_word(line, "ls") || contains_word(line, "list") ||
        contains_phrase(line, "show files")) {
        cmd_ls(); return 1;
    }

    // Processes
    if (contains_word(line, "ps") || contains_phrase(line, "show processes") ||
        contains_phrase(line, "list processes") || contains_phrase(line, "show tasks")) {
        cmd_ps(); return 1;
    }

    if (contains_phrase(line, "create process") || contains_phrase(line, "new process") ||
        contains_phrase(line, "start process")) {
        const char *name = last_word(line);
        if (name && !str_eq(name, "process")) cmd_create_process(name);
        else {
            console_set_color(0xFF5555);
            console_print("Please give a process name.\n");
        }
        return 1;
    }

    if (contains_word(line, "delete") || contains_word(line, "remove")) {
        const char *name = last_word(line);
        if (name) cmd_delete(name);
        return 1;
    }

    if (contains_phrase(line, "create file") || contains_phrase(line, "make file")) {
        const char *name = last_word(line);
        if (name && !str_eq(name, "file")) cmd_create_file(name);
        return 1;
    }

    if (contains_phrase(line, "create folder") || contains_phrase(line, "make folder") ||
        contains_phrase(line, "make a folder")) {
        const char *name = last_word(line);
        if (name && !str_eq(name, "folder")) cmd_mkdir(name);
        return 1;
    }

    if (contains_word(line, "read") || contains_word(line, "open")) {
        const char *name = last_word(line);
        if (name) cmd_read(name);
        return 1;
    }

    if (contains_phrase(line, "go to") || str_starts_with(line, "cd ")) {
        const char *name = str_starts_with(line, "cd ") ? line + 3 : last_word(line);
        while (name && *name == ' ') name++;
        if (name) cmd_cd(name);
        return 1;
    }

    if (contains_phrase(line, "go up") || str_eq(line, "cd ..")) {
        cmd_cd(".."); return 1;
    }

    return 0;
}

static void process_command(char *line) {
    while (*line == ' ') line++;
    if (*line == 0) return;

    if (str_eq(line, "help")) { cmd_help(); return; }
    if (str_eq(line, "clear")) { cmd_clear(); return; }
    if (str_eq(line, "mem") || str_eq(line, "memory")) { cmd_mem(); return; }
    if (str_eq(line, "about")) { cmd_about(); return; }
    if (str_eq(line, "ls") || str_eq(line, "list")) { cmd_ls(); return; }
    if (str_eq(line, "ps")) { cmd_ps(); return; }
    if (str_eq(line, "pwd")) {
        console_set_color(0x55AAFF); fs_print_path(); console_print("\n"); return;
    }

    if (str_starts_with(line, "echo ")) { cmd_echo(line + 5); return; }
    if (str_starts_with(line, "mkdir ")) {
        const char *n = line + 6; while (*n == ' ') n++; cmd_mkdir(n); return;
    }
    if (str_starts_with(line, "cd ")) {
        const char *n = line + 3; while (*n == ' ') n++; cmd_cd(n); return;
    }
    if (str_starts_with(line, "rm ") || str_starts_with(line, "del ")) {
        const char *n = line + 3; while (*n == ' ') n++; cmd_delete(n); return;
    }

    if (try_natural_language(line)) return;

    console_set_color(0xFF5555);
    console_print("I don't understand: \""); console_print(line); console_print("\"\n");
    console_set_color(0xAAAAAA);
    console_print("Type 'help' for options.\n\n");
}

void shell_run(void) {
    console_set_color(0x00FFCC);
    console_print("========================================\n");
    console_print("       New-OS Interactive Shell\n");
    console_print("========================================\n\n");
    console_set_color(0xCCCCCC);
    console_print("New: process support (try 'ps' or 'create process test')\n\n");

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
