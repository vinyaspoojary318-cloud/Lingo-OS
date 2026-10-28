#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "pmm.h"
#include "fs.h"
#include "process.h"
#include "timer.h"
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
static int str_starts_with(const char *s, const char *p) {
    while (*p) { if (*s != *p) return 0; s++; p++; }
    return 1;
}
static char to_lower(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int contains_word(const char *str, const char *word) {
    while (*str) {
        while (*str == ' ') str++;
        if (!*str) break;
        const char *w = word, *s = str;
        while (*w && *s && to_lower(*s) == to_lower(*w)) { s++; w++; }
        if (*w == 0 && (*s == 0 || *s == ' ')) return 1;
        while (*str && *str != ' ') str++;
    }
    return 0;
}
static int contains_phrase(const char *str, const char *phrase) {
    while (*str) {
        const char *p = phrase, *s = str;
        while (*p && *s && to_lower(*s) == to_lower(*p)) { s++; p++; }
        if (*p == 0) return 1;
        str++;
    }
    return 0;
}
static const char *last_word(const char *str) {
    const char *last = str, *s = str;
    while (*s) { if (*s == ' ') last = s + 1; s++; }
    return (last == str || !*last) ? NULL : last;
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
    console_print("Filesystem: ls, create folder/file, write, read, delete, cd\n");
    console_print("Processes:  ps, create process\n");
    console_print("System:     help, clear, mem, about, ticks, elfcheck\n\n");
}

static void cmd_clear(void) { console_clear(0x0A0A1A); }

static void cmd_mem(void) {
    console_set_color(0xCCCCCC);
    console_print("Total "); console_print_dec(pmm_get_total_memory()/1024/1024);
    console_print(" MiB | Free "); console_print_dec(pmm_get_free_memory()/1024/1024);
    console_print(" MiB | Used "); console_print_dec(pmm_get_used_memory()/1024/1024);
    console_print(" MiB\n");
}

static void cmd_about(void) {
    console_set_color(0x00FFCC);
    console_print("New-OS v1.2 — dual-mode foundation + file write + timer\n");
}

static void cmd_ticks(void) {
    console_set_color(0xCCCCCC);
    console_print("Ticks: ");
    console_print_dec(timer_get_ticks());
    console_print("\n");
}

static void cmd_ls(void) { fs_list(); }

static void cmd_mkdir(const char *n) {
    if (!n || !*n) { console_set_color(0xFF5555); console_print("Need name.\n"); return; }
    if (fs_create_folder(n)) { console_set_color(0x55FF55); console_print("Created folder: "); console_print(n); console_print("\n"); }
    else { console_set_color(0xFF5555); console_print("Failed.\n"); }
}

static void cmd_create_file(const char *n) {
    if (!n || !*n) { console_set_color(0xFF5555); console_print("Need name.\n"); return; }
    if (fs_create_file(n, "")) { console_set_color(0x55FF55); console_print("Created file: "); console_print(n); console_print("\n"); }
    else { console_set_color(0xFF5555); console_print("Failed.\n"); }
}

static void cmd_write(const char *name, const char *content) {
    if (!name || !*name) { console_set_color(0xFF5555); console_print("Usage: write <file> <text>\n"); return; }
    if (!content) content = "";
    if (fs_write_file(name, content)) {
        console_set_color(0x55FF55);
        console_print("Wrote to "); console_print(name); console_print("\n");
    } else {
        console_set_color(0xFF5555);
        console_print("Write failed.\n");
    }
}

static void cmd_delete(const char *n) {
    if (!n || !*n) { console_set_color(0xFF5555); console_print("Need name.\n"); return; }
    if (fs_delete(n)) { console_set_color(0x55FF55); console_print("Deleted: "); console_print(n); console_print("\n"); }
    else { console_set_color(0xFF5555); console_print("Failed.\n"); }
}

static void cmd_read(const char *n) {
    if (!n || !*n) { console_set_color(0xFF5555); console_print("Need file.\n"); return; }
    char buf[512];
    if (fs_read_file(n, buf, sizeof(buf))) {
        console_set_color(0xCCCCCC);
        if (!buf[0]) console_print("(empty)\n"); else { console_print(buf); console_print("\n"); }
    } else { console_set_color(0xFF5555); console_print("Not found.\n"); }
}

static void cmd_cd(const char *p) {
    if (!p || !*p) { console_set_color(0xFF5555); console_print("Need folder.\n"); return; }
    if (!fs_change_dir(p)) { console_set_color(0xFF5555); console_print("Not found.\n"); }
}

static void cmd_ps(void) { process_list(); }

static void cmd_create_process(const char *n) {
    if (!n || !*n) { console_set_color(0xFF5555); console_print("Need name.\n"); return; }
    int pid = process_create(n);
    if (pid >= 0) {
        console_set_color(0x55FF55);
        console_print("Process '"); console_print(n); console_print("' PID "); console_print_dec(pid); console_print("\n");
    } else console_set_color(0xFF5555), console_print("Failed.\n");
}

static void cmd_elfcheck(void) {
    console_set_color(0xAAAAFF);
    console_print("[ELF] Loader ready for ELF64 PT_LOAD + user stack.\n");
    console_print("      Provide a real binary to load into Ring 3.\n\n");
}

static int try_nl(const char *line) {
    if (contains_word(line, "help") || contains_phrase(line, "what can you do")) { cmd_help(); return 1; }
    if (contains_word(line, "clear")) { cmd_clear(); return 1; }
    if (contains_word(line, "memory") || contains_word(line, "mem")) { cmd_mem(); return 1; }
    if (contains_word(line, "about")) { cmd_about(); return 1; }
    if (contains_word(line, "ticks") || contains_phrase(line, "show ticks")) { cmd_ticks(); return 1; }
    if (contains_word(line, "ls") || contains_phrase(line, "show files")) { cmd_ls(); return 1; }
    if (contains_word(line, "ps") || contains_phrase(line, "show processes")) { cmd_ps(); return 1; }

    if (contains_phrase(line, "write file") || contains_phrase(line, "write to")) {
        // write file NAME text...  — simplified: last word as name not ideal
        // Prefer: write name text
        console_set_color(0xFFCC55);
        console_print("Use: write <filename> <text>\n");
        return 1;
    }

    if (contains_phrase(line, "create process")) {
        const char *n = last_word(line);
        if (n && !str_eq(n, "process")) cmd_create_process(n);
        return 1;
    }
    if (contains_word(line, "delete") || contains_word(line, "remove")) {
        const char *n = last_word(line);
        if (n) cmd_delete(n);
        return 1;
    }
    if (contains_phrase(line, "create file")) {
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
    if (contains_word(line, "elf")) { cmd_elfcheck(); return 1; }
    return 0;
}

static void process_command(char *line) {
    while (*line == ' ') line++;
    if (!*line) return;

    if (str_eq(line, "help")) { cmd_help(); return; }
    if (str_eq(line, "clear")) { cmd_clear(); return; }
    if (str_eq(line, "mem") || str_eq(line, "memory")) { cmd_mem(); return; }
    if (str_eq(line, "about")) { cmd_about(); return; }
    if (str_eq(line, "ticks")) { cmd_ticks(); return; }
    if (str_eq(line, "ls") || str_eq(line, "list")) { cmd_ls(); return; }
    if (str_eq(line, "ps")) { cmd_ps(); return; }
    if (str_eq(line, "elfcheck") || str_eq(line, "elf")) { cmd_elfcheck(); return; }
    if (str_eq(line, "pwd")) { console_set_color(0x55AAFF); fs_print_path(); console_print("\n"); return; }

    if (str_starts_with(line, "write ")) {
        char *p = line + 6;
        while (*p == ' ') p++;
        char *name = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = 0; p++; while (*p == ' ') p++; }
        cmd_write(name, p);
        return;
    }

    if (str_starts_with(line, "mkdir ")) { const char *n = line+6; while(*n==' ')n++; cmd_mkdir(n); return; }
    if (str_starts_with(line, "cd ")) { const char *n = line+3; while(*n==' ')n++; cmd_cd(n); return; }
    if (str_starts_with(line, "rm ") || str_starts_with(line, "del ")) { const char *n = line+3; while(*n==' ')n++; cmd_delete(n); return; }

    if (try_nl(line)) return;

    console_set_color(0xFF5555);
    console_print("Unknown: \""); console_print(line); console_print("\"\n");
}

void shell_run(void) {
    console_set_color(0x00FFCC);
    console_print("========================================\n");
    console_print("       New-OS Interactive Shell\n");
    console_print("========================================\n\n");
    console_set_color(0xCCCCCC);
    console_print("Try: write hello.txt Hello world\n");
    console_print("     read hello.txt\n");
    console_print("     ticks\n\n");

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
                char b[2] = {c, 0};
                console_print(b);
            }
        }
    }
}
