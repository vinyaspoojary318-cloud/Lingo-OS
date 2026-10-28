#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "pmm.h"
#include "fs.h"
#include "process.h"
#include "elf.h"
#include <stddef.h>
#include <stdint.h>

#define INPUT_MAX 128

static char input_buffer[INPUT_MAX];
static size_t input_len = 0;

static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) { if (*str != *prefix) return 0; str++; prefix++; }
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
    while (*s) { if (*s == ' ') last = s + 1; s++; }
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
    console_print("Commands:\n\n");
    console_set_color(0x00FFCC);
    console_print("Filesystem:  ls, create folder/file, delete, go to, read\n");
    console_print("Processes:   ps, create process\n");
    console_print("ELF:         elfcheck (demo validation)\n");
    console_print("System:      help, clear, mem, about\n\n");
}

static void cmd_clear(void) { console_clear(0x0A0A1A); }

static void cmd_mem(void) {
    console_set_color(0xAAAAFF);
    console_print("Memory:\n");
    console_set_color(0xCCCCCC);
    console_print("  Total: "); console_print_dec(pmm_get_total_memory()/1024/1024);
    console_print(" MiB  Free: "); console_print_dec(pmm_get_free_memory()/1024/1024);
    console_print(" MiB  Used: "); console_print_dec(pmm_get_used_memory()/1024/1024);
    console_print(" MiB\n\n");
}

static void cmd_about(void) {
    console_set_color(0x00FFCC);
    console_print("New-OS v1.1\n");
    console_set_color(0xCCCCCC);
    console_print("Natural-language OS with dual-mode foundation + ELF loader.\n\n");
}

static void cmd_ls(void) { console_set_color(0xCCCCCC); fs_list(); }

static void cmd_mkdir(const char *name) {
    if (!name || !*name) { console_set_color(0xFF5555); console_print("Need folder name.\n"); return; }
    if (fs_create_folder(name)) {
        console_set_color(0x55FF55); console_print("Created folder: "); console_print(name); console_print("\n");
    } else { console_set_color(0xFF5555); console_print("Failed.\n"); }
}

static void cmd_create_file(const char *name) {
    if (!name || !*name) { console_set_color(0xFF5555); console_print("Need file name.\n"); return; }
    if (fs_create_file(name, "")) {
        console_set_color(0x55FF55); console_print("Created file: "); console_print(name); console_print("\n");
    } else { console_set_color(0xFF5555); console_print("Failed.\n"); }
}

static void cmd_delete(const char *name) {
    if (!name || !*name) { console_set_color(0xFF5555); console_print("Need name.\n"); return; }
    if (fs_delete(name)) {
        console_set_color(0x55FF55); console_print("Deleted: "); console_print(name); console_print("\n");
    } else { console_set_color(0xFF5555); console_print("Failed.\n"); }
}

static void cmd_read(const char *name) {
    if (!name || !*name) { console_set_color(0xFF5555); console_print("Need file name.\n"); return; }
    char buf[256];
    if (fs_read_file(name, buf, sizeof(buf))) {
        console_set_color(0xCCCCCC);
        if (!buf[0]) console_print("(empty)\n"); else { console_print(buf); console_print("\n"); }
    } else { console_set_color(0xFF5555); console_print("Not found.\n"); }
}

static void cmd_cd(const char *path) {
    if (!path || !*path) { console_set_color(0xFF5555); console_print("Need folder.\n"); return; }
    if (!fs_change_dir(path)) {
        console_set_color(0xFF5555); console_print("Not found: "); console_print(path); console_print("\n");
    }
}

static void cmd_ps(void) { process_list(); }

static void cmd_create_process(const char *name) {
    if (!name || !*name) { console_set_color(0xFF5555); console_print("Need name.\n"); return; }
    int pid = process_create(name);
    if (pid >= 0) {
        console_set_color(0x55FF55);
        console_print("Created process '"); console_print(name);
        console_print("' PID "); console_print_dec(pid); console_print("\n");
    } else { console_set_color(0xFF5555); console_print("Failed.\n"); }
}

// Demo: create a tiny fake ELF header in memory and validate/load it
static void cmd_elfcheck(void) {
    // Minimal valid ELF64 header (not a full program, just for validation demo)
    static uint8_t fake_elf[64] = {
        0x7F, 'E', 'L', 'F', // magic
        2,                   // class 64
        1,                   // little endian
        1,                   // version
        0,                   // osabi
        0,0,0,0,0,0,0,0,     // padding
        2, 0,                // type = EXEC
        0x3E, 0,             // machine = x86_64
        1, 0, 0, 0,          // version
        // entry (8 bytes) - placeholder
        0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        // phoff
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // rest zeroed for this demo
    };

    console_set_color(0xAAAAFF);
    console_print("[ELF] Running header check on demo buffer...\n");

    if (elf_check(fake_elf, sizeof(fake_elf))) {
        console_set_color(0x55FF55);
        console_print("[ELF] Header looks valid (demo)\n");
    } else {
        console_set_color(0xFFCC55);
        console_print("[ELF] Header check failed (expected for incomplete demo)\n");
    }

    console_set_color(0xCCCCCC);
    console_print("Full ELF load will work when you provide a real ELF64 binary.\n");
    console_print("Loader supports PT_LOAD segments + user stack mapping.\n\n");
}

static int try_natural_language(const char *line) {
    if (contains_word(line, "help") || contains_phrase(line, "what can you do")) { cmd_help(); return 1; }
    if (contains_word(line, "clear") || contains_phrase(line, "clear the screen")) { cmd_clear(); return 1; }
    if (contains_word(line, "memory") || contains_word(line, "mem") || contains_phrase(line, "show memory")) { cmd_mem(); return 1; }
    if (contains_word(line, "about")) { cmd_about(); return 1; }
    if (contains_word(line, "ls") || contains_phrase(line, "show files")) { cmd_ls(); return 1; }
    if (contains_word(line, "ps") || contains_phrase(line, "show processes")) { cmd_ps(); return 1; }

    if (contains_phrase(line, "create process") || contains_phrase(line, "new process")) {
        const char *n = last_word(line);
        if (n && !str_eq(n, "process")) cmd_create_process(n);
        return 1;
    }
    if (contains_word(line, "delete") || contains_word(line, "remove")) {
        const char *n = last_word(line);
        if (n) cmd_delete(n);
        return 1;
    }
    if (contains_phrase(line, "create file") || contains_phrase(line, "make file")) {
        const char *n = last_word(line);
        if (n && !str_eq(n, "file")) cmd_create_file(n);
        return 1;
    }
    if (contains_phrase(line, "create folder") || contains_phrase(line, "make folder")) {
        const char *n = last_word(line);
        if (n && !str_eq(n, "folder")) cmd_mkdir(n);
        return 1;
    }
    if (contains_word(line, "read") || contains_word(line, "open")) {
        const char *n = last_word(line);
        if (n) cmd_read(n);
        return 1;
    }
    if (contains_phrase(line, "go to") || str_starts_with(line, "cd ")) {
        const char *n = str_starts_with(line, "cd ") ? line + 3 : last_word(line);
        while (n && *n == ' ') n++;
        if (n) cmd_cd(n);
        return 1;
    }
    if (contains_phrase(line, "go up") || str_eq(line, "cd ..")) { cmd_cd(".."); return 1; }

    if (contains_word(line, "elf") || contains_phrase(line, "check elf") || contains_phrase(line, "elf check")) {
        cmd_elfcheck(); return 1;
    }

    return 0;
}

static void process_command(char *line) {
    while (*line == ' ') line++;
    if (!*line) return;

    if (str_eq(line, "help")) { cmd_help(); return; }
    if (str_eq(line, "clear")) { cmd_clear(); return; }
    if (str_eq(line, "mem") || str_eq(line, "memory")) { cmd_mem(); return; }
    if (str_eq(line, "about")) { cmd_about(); return; }
    if (str_eq(line, "ls") || str_eq(line, "list")) { cmd_ls(); return; }
    if (str_eq(line, "ps")) { cmd_ps(); return; }
    if (str_eq(line, "elfcheck") || str_eq(line, "elf")) { cmd_elfcheck(); return; }
    if (str_eq(line, "pwd")) { console_set_color(0x55AAFF); fs_print_path(); console_print("\n"); return; }

    if (str_starts_with(line, "mkdir ")) { const char *n = line+6; while(*n==' ')n++; cmd_mkdir(n); return; }
    if (str_starts_with(line, "cd ")) { const char *n = line+3; while(*n==' ')n++; cmd_cd(n); return; }
    if (str_starts_with(line, "rm ") || str_starts_with(line, "del ")) { const char *n = line+3; while(*n==' ')n++; cmd_delete(n); return; }

    if (try_natural_language(line)) return;

    console_set_color(0xFF5555);
    console_print("I don't understand: \""); console_print(line); console_print("\"\n");
}

void shell_run(void) {
    console_set_color(0x00FFCC);
    console_print("========================================\n");
    console_print("       New-OS Interactive Shell\n");
    console_print("========================================\n\n");
    console_set_color(0xCCCCCC);
    console_print("ELF loader ready. Try: elfcheck\n\n");

    print_prompt();

    for (;;) {
        if (!keyboard_has_char()) { asm volatile ("hlt"); continue; }
        char c = keyboard_getchar();
        if (c == '\n') {
            console_print("\n");
            input_buffer[input_len] = 0;
            process_command(input_buffer);
            input_len = 0;
            print_prompt();
        } else if (c == '\b') {
            if (input_len > 0) { input_len--; console_print("\b \b"); }
        } else if (c >= 32 && c < 127) {
            if (input_len < INPUT_MAX - 1) {
                input_buffer[input_len++] = c;
                char buf[2] = {c, 0};
                console_print(buf);
            }
        }
    }
}
