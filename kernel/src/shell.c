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

// ========== Commands ==========

static void cmd_help(void) {
    console_set_color(0x55FF55);
    console_print("Commands & natural language you can use:\n\n");

    console_set_color(0x00FFCC);
    console_print("Filesystem:\n");
    console_set_color(0xCCCCCC);
    console_print("  ls / show files              - List files & folders\n");
    console_print("  cd <name> / go to <name>     - Enter folder\n");
    console_print("  mkdir <name>                 - Create folder\n");
    console_print("  create folder <name>         - Create folder\n");
    console_print("  create file <name>           - Create empty file\n");
    console_print("  delete <name> / remove       - Delete file or empty folder\n");
    console_print("  read <name> / open <name>    - Show file content\n\n");

    console_set_color(0x00FFCC);
    console_print("System:\n");
    console_set_color(0xCCCCCC);
    console_print("  help / what can you do\n");
    console_print("  clear / clear the screen\n");
    console_print("  mem / show memory\n");
    console_print("  about\n\n");
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
    console_print("New-OS v0.7\n");
    console_set_color(0xCCCCCC);
    console_print("Natural-language-first operating system.\n\n");
    console_print("Features: memory manager, interrupts, keyboard,\n");
    console_print("shell, natural language, in-memory filesystem\n");
    console_print("(create/list/delete folders & files)\n\n");
}

static void cmd_echo(const char *args) {
    if (args && *args) { console_print(args); console_print("\n"); }
    else console_print("\n");
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
    if (str_eq(name, "folder") || str_eq(name, "directory") ||
        str_eq(name, "a") || str_eq(name, "the")) {
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
        console_print("Created file: ");
        console_print(name);
        console_print("\n");
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
        console_print("Deleted: ");
        console_print(name);
        console_print("\n");
    } else {
        console_set_color(0xFF5555);
        console_print("Could not delete (not found or folder not empty).\n");
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
        if (buf[0] == 0) {
            console_print("(empty file)\n");
        } else {
            console_print(buf);
            console_print("\n");
        }
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
        console_print("Folder not found: ");
        console_print(path);
        console_print("\n");
    }
}

// ========== Natural Language ==========

static int try_natural_language(const char *line) {
    if (contains_word(line, "help") || contains_phrase(line, "what can you do") ||
        contains_phrase(line, "what can i do") || contains_phrase(line, "show commands")) {
        cmd_help(); return 1;
    }

    if (contains_word(line, "clear") || contains_word(line, "clean") ||
        contains_phrase(line, "clear screen") || contains_phrase(line, "clear the screen")) {
        cmd_clear(); return 1;
    }

    if (contains_word(line, "memory") || contains_word(line, "mem") ||
        contains_phrase(line, "how much memory") || contains_phrase(line, "show memory") ||
        contains_word(line, "ram")) {
        cmd_mem(); return 1;
    }

    if (contains_word(line, "about") || contains_phrase(line, "who are you") ||
        contains_phrase(line, "what is this") || contains_phrase(line, "tell me about")) {
        cmd_about(); return 1;
    }

    if (contains_word(line, "ls") || contains_word(line, "list") ||
        contains_phrase(line, "show files") || contains_phrase(line, "show folders") ||
        contains_phrase(line, "list files") || contains_phrase(line, "what is here")) {
        cmd_ls(); return 1;
    }

    // Delete / remove
    if (contains_word(line, "delete") || contains_word(line, "remove") ||
        contains_phrase(line, "delete file") || contains_phrase(line, "delete folder") ||
        contains_phrase(line, "remove file") || contains_phrase(line, "remove folder")) {
        const char *name = last_word(line);
        if (name && !str_eq(name, "file") && !str_eq(name, "folder") &&
            !str_eq(name, "delete") && !str_eq(name, "remove")) {
            cmd_delete(name);
        } else {
            console_set_color(0xFF5555);
            console_print("Please say what to delete.\n");
        }
        return 1;
    }

    // Create file
    if (contains_phrase(line, "create file") || contains_phrase(line, "make file") ||
        contains_phrase(line, "new file") || contains_phrase(line, "make a file")) {
        const char *name = last_word(line);
        if (name && !str_eq(name, "file") && !str_eq(name, "a")) {
            cmd_create_file(name);
        } else {
            console_set_color(0xFF5555);
            console_print("Please give a file name.\n");
        }
        return 1;
    }

    // Create folder
    if (contains_phrase(line, "create folder") || contains_phrase(line, "make folder") ||
        contains_phrase(line, "make a folder") || contains_phrase(line, "create a folder") ||
        contains_phrase(line, "new folder") || contains_phrase(line, "make directory") ||
        contains_phrase(line, "create directory")) {

        const char *name = NULL;
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
        if (!name) {
            name = last_word(line);
            if (name && (str_eq(name, "folder") || str_eq(name, "directory") ||
                         str_eq(name, "a") || str_eq(name, "the"))) name = NULL;
        }
        cmd_mkdir(name);
        return 1;
    }

    // Read / open file
    if (contains_word(line, "read") || contains_word(line, "open") ||
        contains_phrase(line, "show content") || contains_phrase(line, "read file")) {
        const char *name = last_word(line);
        if (name && !str_eq(name, "file") && !str_eq(name, "read") && !str_eq(name, "open")) {
            cmd_read(name);
        } else {
            console_set_color(0xFF5555);
            console_print("Please give a file name.\n");
        }
        return 1;
    }

    // Change directory
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

    if (contains_phrase(line, "go up") || contains_phrase(line, "go back") ||
        contains_phrase(line, "parent folder") || str_eq(line, "cd ..")) {
        cmd_cd("..");
        return 1;
    }

    return 0;
}

// ========== Command processor ==========

static void process_command(char *line) {
    while (*line == ' ') line++;
    if (*line == 0) return;

    if (str_eq(line, "help"))   { cmd_help(); return; }
    if (str_eq(line, "clear"))  { cmd_clear(); return; }
    if (str_eq(line, "mem") || str_eq(line, "memory")) { cmd_mem(); return; }
    if (str_eq(line, "about"))  { cmd_about(); return; }
    if (str_eq(line, "ls") || str_eq(line, "list")) { cmd_ls(); return; }
    if (str_eq(line, "pwd")) {
        console_set_color(0x55AAFF);
        fs_print_path();
        console_print("\n");
        return;
    }

    if (str_starts_with(line, "echo ")) { cmd_echo(line + 5); return; }
    if (str_eq(line, "echo")) { cmd_echo(""); return; }

    if (str_starts_with(line, "mkdir ")) {
        const char *n = line + 6; while (*n == ' ') n++;
        cmd_mkdir(n); return;
    }
    if (str_starts_with(line, "cd ")) {
        const char *n = line + 3; while (*n == ' ') n++;
        cmd_cd(n); return;
    }
    if (str_eq(line, "cd")) { cmd_cd("/"); return; }

    if (str_starts_with(line, "rm ") || str_starts_with(line, "del ")) {
        const char *n = line + 3; while (*n == ' ') n++;
        cmd_delete(n); return;
    }
    if (str_starts_with(line, "read ") || str_starts_with(line, "cat ")) {
        const char *n = line + 4; while (*n == ' ') n++;
        cmd_read(n); return;
    }

    if (try_natural_language(line)) return;

    console_set_color(0xFF5555);
    console_print("I don't understand: \"");
    console_print(line);
    console_print("\"\n");
    console_set_color(0xAAAAAA);
    console_print("Type 'help' or 'what can you do'.\n\n");
}

void shell_run(void) {
    console_set_color(0x00FFCC);
    console_print("========================================\n");
    console_print("       New-OS Interactive Shell\n");
    console_print("========================================\n\n");
    console_set_color(0xCCCCCC);
    console_print("Try:  create folder Work\n");
    console_print("      create file notes.txt\n");
    console_print("      show files\n");
    console_print("      delete notes.txt\n\n");

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
